/*
 * usbfunc.c
 *
 *  Created on: August 1, 2012
 *      Author: rocky
 */
#include <linux/usb.h>
#include "kdriver.h"
#include "hw.h"

// -----------------------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------------------
#define DO_CTRL_XFER_LOCKED(daq_dev, _expr_) \
   ({\
      int __xret;\
      mutex_lock(&daq_dev->ctrl_pipe_lock);\
      if (likely(daq_dev->udev)){\
         __xret = _expr_;\
      } else {\
         __xret = -ENODEV;\
      }\
      mutex_unlock(&daq_dev->ctrl_pipe_lock);\
      __xret;\
   })

static inline
int __daq_usb_control_in_idx(struct usb_device *usb_dev, __u8 major, __u16 minor, __u16 idx, void *data, __u16 size)
{
   return usb_control_msg(usb_dev, usb_rcvctrlpipe(usb_dev, 0),
            major, USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
            minor, idx, data, size, USB_CTRL_GET_TIMEOUT);
}

static inline
int __daq_usb_control_in(struct usb_device *usb_dev, __u8 major, __u16 minor, void *data, __u16 size)
{
   return __daq_usb_control_in_idx(usb_dev, major, minor, 0, data, size);
}

static inline
int __daq_usb_control_out_idx(struct usb_device *usb_dev, __u8 major, __u16 minor, __u16 idx, void *data, __u16 size)
{
   return usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0),
            major, USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
            minor, idx, data, size,  USB_CTRL_SET_TIMEOUT);
}

static inline
int __daq_usb_control_out(struct usb_device *usb_dev, __u8 major, __u16 minor, void *data, __u16 size)
{
   return __daq_usb_control_out_idx(usb_dev, major, minor, 0, data, size);
}

// -----------------------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------------------
int daq_usb_dev_get_firmware_ver(daq_device_t *daq_dev, char ver[], int len)
{
   return DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_in(daq_dev->udev, MAJOR_SYSTEM, MINOR_GET_FW_VERSION, ver, len));
}

int daq_usb_dev_init_firmware(daq_device_t *daq_dev, int is_open)
{
   __u32 dummy = 0; // Unused, but firmware required.

   return DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_out(daq_dev->udev, MAJOR_SYSTEM,
               is_open ? MINOR_DEVICE_OPEN : MINOR_DEVICE_CLOSE, &dummy, sizeof(dummy)));
}

int daq_usb_dev_locate_device(daq_device_t *daq_dev, __u32 enable)
{
   return DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_out(daq_dev->udev, MAJOR_SYSTEM, MINOR_LOCATE, &enable, sizeof(enable)));
}

int daq_usb_dev_get_board_id_oscsd(daq_device_t *daq_dev, __u32 *id, __u8 *aiChType, __u8 *oscillator)
{
   __u32 bidOscSd;
   int ret = DO_CTRL_XFER_LOCKED(daq_dev,
               __daq_usb_control_in(daq_dev->udev, MAJOR_SYSTEM, MINOR_READ_SWITCHID, &bidOscSd, sizeof(__u32)));
   *id = bidOscSd & 0xf;
   *aiChType = (__u8)(bidOscSd & 0x10)>>4;
   *oscillator = (__u8)(bidOscSd & 0x20)>>5;

   return ret;
}

int daq_usb_dev_set_board_id_oscsd(daq_device_t *daq_dev, __u32 id, __u8 aiChType, __u8 oscillator)
{
   //daq_trace(("@Set Board ID: %d, AiChType=0x%x, OSC=0x%x\n", id, aiChType, oscillator));

   id &= 0xf;
   id |= ( (aiChType & 0x1) << 4 );
   id |= ( (oscillator & 0x1) << 5 );
   id = __cpu_to_be32(id);

   return DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_out(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_WRITE, &id, sizeof(__u32)));
}

int daq_usb_dev_get_flag(daq_device_t *daq_dev, __u32 *flag)
{
   return DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_in(daq_dev->udev, MAJOR_SYSTEM, MINOR_GET_USB_FLAG, flag, sizeof(__u32)));
}

int daq_usb_dev_dbg_input(daq_device_t *daq_dev, __u16 majorCmd, __u16 minorCmd, __u32 dataSize, void *data)
{
   return DO_CTRL_XFER_LOCKED(daq_dev,
             __daq_usb_control_in(daq_dev->udev, majorCmd, minorCmd, data, dataSize));
}

int daq_usb_dev_dbg_output(daq_device_t *daq_dev, __u16 majorCmd, __u16 minorCmd, __u32 dataSize, void *data)
{
   return DO_CTRL_XFER_LOCKED(daq_dev,
             __daq_usb_control_out(daq_dev->udev, majorCmd, minorCmd, data, dataSize));
}

int daq_usb_get_last_error(daq_device_t *daq_dev, __u32 *error)
{
   __u32 val = 0;
   int ret;

   ret = DO_CTRL_XFER_LOCKED(daq_dev,
             __daq_usb_control_in(daq_dev->udev, MAJOR_SYSTEM, MINOR_GET_LAST_ERROR, &val, sizeof(val)));

   if (ret > 0){
      *error = __be32_to_cpu(val);
   }
   return ret;
}

// -----------------------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------------------
int daq_usb_ai_configure_channel(daq_device_t *daq_dev, __u32 phyChan, __u8 chanType, __u8 gain)
{
   __u32 data = ( chanType && (gain == 0) ) ? 7 : gain;
   //daq_trace(("UsbSetAiChanCfg, phyChan:%d, gain:%d\n", phyChan, data ));
   if (chanType){
      data += 0x100;
   }
   data = __cpu_to_be32(data);

   return DO_CTRL_XFER_LOCKED(daq_dev,
             __daq_usb_control_out_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_WRITE, ADn_IN(phyChan), &data, sizeof(data)));
}

int daq_usb_ai_read_channel(daq_device_t *daq_dev, __u32 phyChan, __u16 *sample)
{
   int      ret;
   __u32    trigMode = 0;
   __u32    chData;

   trigMode = __cpu_to_be32(trigMode);

   ret = DO_CTRL_XFER_LOCKED(daq_dev,\
         ({
            ret = __daq_usb_control_out_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_WRITE, SET_TRIGERMODE, &trigMode, sizeof(trigMode));
            if (ret > 0) {
               ret = __daq_usb_control_in_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_READ, ADn_IN(phyChan), &chData, sizeof(chData));
            }
            ret;
         }));

   if (ret > 0){
      *sample = (__u16)__be32_to_cpu(chData);
   }
   return ret;
}

int daq_usb_fai_get_cali_info(daq_device_t *daq_dev, __u32 gain, __u32 chan, __u8 *offset, __u8 *span)
{
   int ret;
   __u32 calibInfo;

   ret = DO_CTRL_XFER_LOCKED(daq_dev,
                __daq_usb_control_in_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_READ, ADn_CALI_GET_Gm(chan, gain), &calibInfo, sizeof(calibInfo)));
   if (ret > 0){
      calibInfo = __be32_to_cpu(calibInfo);
      *offset = (__u8)( (calibInfo & 0xffff) >> 8 );
      *span   = (__u8)( calibInfo & 0xff );
   }
   return ret;
}

int daq_usb_fai_start(daq_device_t *daq_dev, __u16 phyChanStart, __u16 logChanCount, __u16 trigSrc, __u32 dataCount,
                      __u16 cyclic, __u32 sampleRate, __u16 chanConfig, __u16 phyChanEnd, __u8 *gain)
{
   int ret;
   __u32 phyChanCount, chanProp, devProp, convNum, startCmd;
   phyChanCount = chanConfig ? logChanCount * 2 : logChanCount;
   //daq_trace(("phyChanStart=%d, phyChanEnd=%d, phyChanCount= %d\n", phyChanStart, phyChanEnd, phyChanCount));

   chanProp = phyChanStart;
   chanProp <<= 24;
   chanProp += phyChanCount << 16;
   if (chanConfig) // DIFF
   {
      chanProp += 0x100;
      chanProp += (gain[phyChanStart] == 0 ? 7 : gain[phyChanStart]);
   } else {
      chanProp += gain[phyChanStart];
   }
   chanProp = __cpu_to_be32(chanProp);

   ret = DO_CTRL_XFER_LOCKED(daq_dev,
                __daq_usb_control_out_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_WRITE, FAI_CHANNEL_PROPERY_SET, &chanProp, sizeof(chanProp)));
   if (ret < 0)
      return ret;

   // Device Property set
   devProp = 0;
   devProp += sampleRate;
   if ( cyclic )
   {
      devProp += 0x01000000;
   }
   if ( trigSrc )
   {
      devProp += 0x00010000;
   }
   devProp = __cpu_to_be32(devProp);

   ret = DO_CTRL_XFER_LOCKED(daq_dev,
                __daq_usb_control_out_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_WRITE, FAI_DEV_PROPERY_SET, &devProp, sizeof(devProp)));
   if (ret < 0)
      return ret;

   // Set Conversion number
   convNum = dataCount;
   convNum = __cpu_to_be32(convNum);

   ret = DO_CTRL_XFER_LOCKED(daq_dev,
                __daq_usb_control_out_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_WRITE, FAI_CONVNUM_SET, &convNum, sizeof(convNum)));
   if (ret < 0)
      return ret;

   // Start
   startCmd = 0;
   return DO_CTRL_XFER_LOCKED(daq_dev,
                __daq_usb_control_out_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_WRITE, FAI_START, &startCmd, sizeof(startCmd)));
}

int daq_usb_fai_stop(daq_device_t *daq_dev)
{
   __u32 stopCmd = 0;
   return DO_CTRL_XFER_LOCKED(daq_dev,
             __daq_usb_control_out_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_WRITE, FAI_STOP, &stopCmd, sizeof(stopCmd)));
}

// -----------------------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------------------
int daq_usb_ao_write_channel(daq_device_t *daq_dev, __u32 phyChan, __u32 data)
{
   __u32 binData = data;
   //daq_trace(("UsbWriteAoSamples, phyChan:%d, data = %d\n", phyChan, data ));
   binData = __cpu_to_be32(binData);

   return DO_CTRL_XFER_LOCKED(daq_dev,
             __daq_usb_control_out_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_WRITE, (__u16)DAn_OUT(phyChan), &binData, sizeof(binData)));
}

// -----------------------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------------------
int daq_usb_di_read_port(daq_device_t *daq_dev, __u8 *data)
{
   __u32  diData;
   int ret;

   ret = DO_CTRL_XFER_LOCKED(daq_dev,
             __daq_usb_control_in_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_READ, DI_IN, &diData, sizeof(diData)));

   if (ret > 0){
      *data = (__u8)__be32_to_cpu(diData);
   }
   return ret;
}

int daq_usb_do_write_port(daq_device_t *daq_dev, __u8 *data)
{
   __u32  doData = *data;
   doData = __cpu_to_be32(doData);

   return DO_CTRL_XFER_LOCKED(daq_dev,
                __daq_usb_control_out_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_WRITE, DO_OUT, &doData, sizeof(doData)));
}

int daq_usb_do_read_port(daq_device_t *daq_dev, __u8 *data)
{
   __u32  doData;
   int ret;

   ret = DO_CTRL_XFER_LOCKED(daq_dev,
             __daq_usb_control_in_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_READ, DO_STATUS_GET, &doData, sizeof(doData)));

   if (ret > 0){
      *data = (__u8)__be32_to_cpu(doData);
   }
   return ret;
}

// -----------------------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------------------
int daq_usb_cntr_get_base_clk(daq_device_t *daq_dev, __u32 *clk)
{
   return DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_in(daq_dev->udev, MAJOR_COUNTER, MINOR_CNT_GET_BASECLK, clk, sizeof(*clk)));
}

int daq_usb_cntr_reset(daq_device_t *daq_dev, __u32 counter)
{
   __u32 dummy = 0;

   return DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_out_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_WRITE, COUNTER_STOP, &dummy, sizeof(dummy)));
}

int daq_usb_cntr_start_event_count(daq_device_t *daq_dev, __u32 counter)
{
   __u32 dummy = 0;

   return DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_out_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_WRITE, COUNTER_START, &dummy, sizeof(dummy)));
}

int daq_usb_cntr_read_event_count_aysn(daq_device_t *daq_dev, __u32 counter, __u32 *cntrValue)
{
   return DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_in_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_READ, COUNTER_READ, cntrValue, sizeof(__u32)));
}

int daq_usb_cntr_read_event_count(daq_device_t *daq_dev, __u32 counter, __u32 *cntrValue, __u16 *overflow)
{
   __u32 count;
   __u32 overFlow;
   int ret;

   ret = DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_in_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_READ, COUNTER_CHECK, &overFlow, sizeof(overFlow)));
   if (ret < 0){
      return ret;
   }

   ret = DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_in_idx(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_READ, COUNTER_READ, &count, sizeof(count)));
   if (ret < 0){
         return ret;
   }

   count = __be32_to_cpu(count);
   overFlow = __be32_to_cpu(overFlow);
   overFlow &= 0xffff;
   //daq_trace(("UsbReadEventCount(after Swap): count=0x%x, overflow=0x%x\n", count, overFlow));

   *cntrValue = count;
   *overflow = overFlow ? 1 : 0;

   return ret;
}

