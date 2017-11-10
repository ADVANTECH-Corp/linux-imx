/*
 * kdriver.h
 *
 *  Created on: 2013-6-15
 *      Author: 
 */

#ifndef _KERNEL_MODULE_H_
#define _KERNEL_MODULE_H_

#if 0
//#ifndef CONFIG_USB // disable for debain
#  error "This driver needs to have USB support."
#endif

#include <linux/usb.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/workqueue.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <adv/bdaqdef.h>
#include <adv/linux/ioctls.h>
#include <adv/linux/biokernbase.h>
#include "kshared.h"

typedef struct daq_file_ctx{
   struct list_head   ctx_list;
   struct daq_device  *daq_dev;
   HANDLE             events[KrnlSptedEventCount];
   int                write_access;
   int                busy;
}daq_file_ctx_t;

typedef struct daq_device
{
   DEVICE_SHARED        shared;
   struct cdev          cdev;
   struct usb_device    *udev;
   struct usb_interface *iface;
   struct mutex         ctrl_pipe_lock;
   struct usb_endpoint_descriptor *xfer_epd;
   daq_usb_reader_t     usb_reader;

   spinlock_t           dev_lock;

   struct list_head     file_ctx_list;
   daq_file_ctx_t       *file_ctx_pool;
   int                  file_ctx_pool_size;
}daq_device_t;


/************************************************************************/
/* Functions                                                            */
/************************************************************************/
//
// init.c
//
void daq_device_cleanup(daq_device_t * daq_dev);

//
// fops.c
//
int  daq_file_open(struct inode *in, struct file *fp);
int  daq_file_close(struct inode *inode, struct file *filp);
int  daq_file_mmap(struct file *filp, struct vm_area_struct *vma);
long daq_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
int  daq_device_signal_event(daq_device_t *daq_dev, unsigned index);
int  daq_device_clear_event(daq_device_t *daq_dev, unsigned index);

//
// dio.c
//
void daq_dio_initialize_hw(daq_device_t *daq_dev);
int daq_ioctl_di_read_port(daq_device_t *daq_dev, unsigned long arg);
int daq_ioctl_do_write_port(daq_device_t *daq_dev, unsigned long arg);
int daq_ioctl_do_read_port(daq_device_t *daq_dev, unsigned long arg);
int daq_ioctl_diint_set_param(daq_device_t *daq_dev, unsigned long arg);
int daq_ioctl_di_start_snap(daq_device_t *daq_dev, unsigned long arg);
int daq_ioctl_di_stop_snap(daq_device_t *daq_dev, unsigned long arg);
int daq_ioctl_do_write_bit(daq_device_t *daq_dev, unsigned long arg);

//
// usbfunc.c
//
int daq_usb_dev_get_firmware_ver(daq_device_t *daq_dev, char ver[], int len);
int daq_usb_dev_init_firmware(daq_device_t *daq_dev, int is_open);
int daq_usb_dev_locate_device(daq_device_t *daq_dev, __u8 enable);
int daq_usb_dev_get_board_id(daq_device_t *daq_dev, __u32 *id);
int daq_usb_dev_set_board_id(daq_device_t *daq_dev, __u32 id);
int daq_usb_dev_dbg_input(daq_device_t *daq_dev, __u16 majorCmd, __u16 minorCmd, __u32 dataSize, void *data);
int daq_usb_dev_dbg_output(daq_device_t *daq_dev, __u16 majorCmd, __u16 minorCmd, __u32 dataSize, void *data);

int daq_usb_di_read_port(daq_device_t *daq_dev, __u32 start, __u32 count, __u8 *data);
int daq_usb_do_write_port(daq_device_t *daq_dev, __u32 start, __u32 count, __u8 *data);
int daq_usb_do_read_port(daq_device_t *daq_dev, __u32 start, __u32 count, __u8 *data);
int daq_usb_enable_hw_event(daq_device_t *daq_dev, __u32 src_idx, int enable, __u8 gate_ctrl, __u8 trig_edge);

#endif /* _KERNEL_MODULE_H_ */
