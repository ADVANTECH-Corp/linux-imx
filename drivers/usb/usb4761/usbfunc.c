/*
 * usbfunc.c
 *
 *  Created on: 2013-6-15
 *      Author: 
 */
#include <linux/usb.h>
#include "kdriver.h"
#include "hw.h"
#include <adv/hw/daqusb.h>

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
int __daq_usb_control_in(struct usb_device *usb_dev, __u8 major, __u16 minor, void *data, __u16 size)
{
   return usb_control_msg(usb_dev, usb_rcvctrlpipe(usb_dev, 0),
            major, USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
            minor, 0, data, size, USB_CTRL_GET_TIMEOUT);
}

static inline
int __daq_usb_control_out(struct usb_device *usb_dev, __u8 major, __u16 minor, void *data, __u16 size)
{
   return usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0),
            major, USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
            minor, 0, data, size,  USB_CTRL_SET_TIMEOUT);
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

int daq_usb_dev_locate_device(daq_device_t *daq_dev, __u8 enable)
{
   __u32 dummy = 0;

   return DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_out(daq_dev->udev, MAJOR_SYSTEM, MINOR_LOCATE, &dummy, sizeof(dummy)));
}

int daq_usb_dev_get_board_id(daq_device_t *daq_dev, __u32 *id)
{
   int ret = DO_CTRL_XFER_LOCKED(daq_dev,
               __daq_usb_control_in(daq_dev->udev, MAJOR_SYSTEM, MINOR_READ_SWITCHID, id, sizeof(__u32)));
   *id &= 0xf;

   return ret;
}

int daq_usb_dev_set_board_id(daq_device_t *daq_dev, __u32 id)
{
   return DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_out(daq_dev->udev, MAJOR_SYSTEM, MINOR_WRITE_SWITCHID, &id, sizeof(__u32)));
}

int daq_usb_read_eeprom(daq_device_t *daq_dev, __u16 addr, __u32 buflen, void *buffer)
{
   int ret;
   EE_READ ee_read = {0};

   if (buflen < sizeof(__u32)) {
      return -EINVAL;
   }
   return DO_CTRL_XFER_LOCKED(daq_dev,\
   ({
      do {
         ret = __daq_usb_control_out(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_EE_READ_TX, &addr, sizeof(__u16));
         if (ret >= 0){
            ret = __daq_usb_control_in(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_EE_READ_RX, buffer, buflen);
            if (ret >= 0) *(__u32 *)buffer = ee_read.Data;
         }
      } while(0);
      ret;
   }));
}

int daq_usb_write_eeprom(daq_device_t *daq_dev, __u16 addr, __u16 data)
{
   EE_WRITE tx = { data, addr };

   return DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_out(daq_dev->udev, MAJOR_DIRECT_IO, MINOR_DIRECT_EE_WRITE_TX, &tx, sizeof(tx)));
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

// -----------------------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------------------
int daq_usb_di_read_port(daq_device_t *daq_dev, __u32 start, __u32 count, __u8 *data)
{
   BioUsbDI_TX tx = { 0, 1 };
   BioUsbDI_RX rx = { 0 };
   int ret = 0;
   return DO_CTRL_XFER_LOCKED(daq_dev,\
   ({
      while (count--){
         tx.Channel = start++;
         ret = __daq_usb_control_out(daq_dev->udev, MAJOR_DIO, MINOR_DI_TX, &tx, sizeof(tx));
         if (ret < 0) break;

         ret = __daq_usb_control_in(daq_dev->udev, MAJOR_DIO, MINOR_DI_RX, &rx, sizeof(rx));
         if (ret < 0) break;
         *data++ = (__u8)rx.Data;
         start %= DIO_PORT_COUNT;
      }
      ret;
   }));
}

int daq_usb_do_write_port(daq_device_t *daq_dev, __u32 start, __u32 count, __u8 *data)
{
   BioUsbDO tx  = {0, 1};
   int      ret = 0;

   return DO_CTRL_XFER_LOCKED(daq_dev,\
   ({
      while (count--){
         tx.Channel = start++;
         tx.Data    = *data++;
         ret = __daq_usb_control_out(daq_dev->udev, MAJOR_DIO, MINOR_DO, &tx, sizeof(tx));
         if (ret < 0) break;
         start %= DIO_PORT_COUNT;
      }
      ret;
   }));
}

int daq_usb_do_read_port(daq_device_t *daq_dev, __u32 start, __u32 count, __u8 *data)
{
   BioUsbDORead_TX tx = { 0, 1 };
   BioUsbDORead_RX rx = { 0 };
   int             ret= 0;

   return DO_CTRL_XFER_LOCKED(daq_dev,\
   ({
      while (count--){
         tx.Channel = start++;
         ret = __daq_usb_control_out(daq_dev->udev, MAJOR_DIO, MINOR_DO_READ_TX, &tx, sizeof(tx));
         if (ret < 0) break;

         ret = __daq_usb_control_in(daq_dev->udev, MAJOR_DIO, MINOR_DO_READ_RX, &rx, sizeof(rx));
         if (ret < 0) break;
         *data++ = (__u8)rx.Data;
         start %= DIO_PORT_COUNT;
      }
      ret;
   }));
}

int daq_usb_enable_hw_event(daq_device_t *daq_dev, __u32 src_idx, int enable, __u8 gate_ctrl, __u8 trig_edge)
{
   BioUsbEnableEvent tx = { (__u16)src_idx, (__u16)enable, trig_edge};

   return DO_CTRL_XFER_LOCKED(daq_dev,
            __daq_usb_control_out(daq_dev->udev, MAJOR_EVENT, MINOR_EVENT_ENABLE, &tx, sizeof(tx)));
}
