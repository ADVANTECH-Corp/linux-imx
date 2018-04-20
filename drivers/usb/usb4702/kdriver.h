/*
 * kdriver.h
 *
 *  Created on: August 1, 2012
 *      Author: rocky
 */

#ifndef _KERNEL_MODULE_H_
#define _KERNEL_MODULE_H_

#if 0
//#ifndef CONFIG_USB   // disable for debain
#  error "This driver needs to have USB support."
#endif

#include <linux/usb.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/workqueue.h>
#include <asm/uaccess.h>
#include <asm/io.h>
//#include <asm/i387.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <adv/bdaqdef.h>
#include <adv/linux/ioctls.h>
#include <adv/linux/biokernbase.h>

#include "kshared.h"
#include <adv/hw/daqusb.h>


#define FAI_PACKET_NUM   4
#define FAI_PACKET_SIZE  PAGE_SIZE

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

   spinlock_t           fai_lock;
   daq_umem_t           fai_buffer;
   wait_queue_head_t    fai_queue;
   struct work_struct   fai_check_work;
   struct work_struct   fai_stop_work;

   struct delayed_work  cntr_fm_work;
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
// ai.c
//
void daq_ai_initialize_hw(daq_device_t *daq_dev);
void daq_fai_stop_acquisition(daq_device_t *daq_dev, int cleanup);
void daq_fai_check_work_func(struct work_struct *work);
void daq_fai_stop_work_func(struct work_struct *work);
int  daq_ioctl_ai_set_channel(daq_device_t *daq_dev, unsigned long arg);
int  daq_ioctl_ai_read_sample(daq_device_t *daq_dev, unsigned long arg);
int  daq_ioctl_fai_set_param(daq_device_t *daq_dev, unsigned long arg);
int  daq_ioctl_fai_set_buffer(daq_device_t *daq_dev, unsigned long arg);
int  daq_ioctl_fai_start(daq_device_t *daq_dev, unsigned long arg);
int  daq_ioctl_fai_stop(daq_device_t *daq_dev, unsigned long arg);

//
// ao.c
//
void daq_ao_initialize_hw(daq_device_t *daq_dev);
int  daq_ioctl_ao_set_channel(daq_device_t *daq_dev, unsigned long arg);
int  daq_ioctl_ao_write_sample(daq_device_t *daq_dev, unsigned long arg);

//
// dio.c
//
void daq_dio_initialize_hw(daq_device_t *daq_dev);
int daq_ioctl_di_read_port(daq_device_t *daq_dev, unsigned long arg);
int daq_ioctl_do_write_port(daq_device_t *daq_dev, unsigned long arg);
int daq_ioctl_do_read_port(daq_device_t *daq_dev, unsigned long arg);
int daq_ioctl_do_write_bit(daq_device_t *daq_dev, unsigned long arg);

//
// counter.c
//
void daq_cntr_update_state_work_func(struct work_struct *work);
//void daq_cntr_freq_measure_work_func(struct work_struct *work);
void daq_cntr_reset(daq_device_t *daq_dev, __u32 start, __u32 count);
int daq_ioctl_cntr_set_param(daq_device_t *daq_dev, unsigned long arg);
int daq_ioctl_cntr_start(daq_device_t *daq_dev, unsigned long arg);
int daq_ioctl_cntr_read(daq_device_t *daq_dev, unsigned long arg);
int daq_ioctl_cntr_reset(daq_device_t *daq_dev, unsigned long arg);

//
// usbfunc.c
//
int daq_usb_dev_get_firmware_ver(daq_device_t *daq_dev, char ver[], int len);
int daq_usb_dev_init_firmware(daq_device_t *daq_dev, int is_open);
int daq_usb_dev_locate_device(daq_device_t *daq_dev, __u32 enable);
int daq_usb_dev_get_board_id_oscsd(daq_device_t *daq_dev, __u32 *id, __u8 *aiChType, __u8 *oscillator);
int daq_usb_dev_set_board_id_oscsd(daq_device_t *daq_dev, __u32 id, __u8 aiChType, __u8 oscillator);
int daq_usb_dev_get_flag(daq_device_t *daq_dev, __u32 *flag);
int daq_usb_dev_dbg_input(daq_device_t *daq_dev, __u16 majorCmd, __u16 minorCmd, __u32 dataSize, void *data);
int daq_usb_dev_dbg_output(daq_device_t *daq_dev, __u16 majorCmd, __u16 minorCmd, __u32 dataSize, void *data);
int daq_usb_get_last_error(daq_device_t *daq_dev, __u32 *error);

int daq_usb_ai_configure_channel(daq_device_t *daq_dev, __u32 phyChan, __u8 chanType, __u8 gain);
int daq_usb_ai_read_channel(daq_device_t *daq_dev, __u32 phyChan, __u16 *sample);
int daq_usb_fai_get_cali_info(daq_device_t *daq_dev, __u32 gain, __u32 chan, __u8 *offset, __u8 *span);
int daq_usb_fai_start(daq_device_t *daq_dev, __u16 phyChanStart, __u16 logChanCount, __u16 trigSrc, __u32 dataCount,
                      __u16 cyclic, __u32 sampleRate, __u16 chanConfig, __u16 phyChanEnd, __u8 *gain);
int daq_usb_fai_stop(daq_device_t *daq_dev);

int daq_usb_ao_write_channel(daq_device_t *daq_dev, __u32 phyChan, __u32 data);

int daq_usb_di_read_port(daq_device_t *daq_dev, __u8 *data);
int daq_usb_do_write_port(daq_device_t *daq_dev, __u8 *data);
int daq_usb_do_read_port(daq_device_t *daq_dev, __u8 *data);
int daq_usb_enable_hw_event(daq_device_t *daq_dev, __u32 src_idx, int enable, __u8 gate_ctrl, __u8 trig_edge);

int daq_usb_cntr_get_base_clk(daq_device_t *daq_dev, __u32 *clk);
int daq_usb_cntr_reset(daq_device_t *daq_dev, __u32 counter);
int daq_usb_cntr_start_event_count(daq_device_t *daq_dev, __u32 counter);
int daq_usb_cntr_read_event_count_aysn(daq_device_t *daq_dev, __u32 counter, __u32 *cntrValue);
int daq_usb_cntr_read_event_count(daq_device_t *daq_dev, __u32 counter, __u32 *cntrValue, __u16 *overflow);
int daq_usb_cntr_read_freq_measure(daq_device_t *daq_dev, __u32 counter, BioUsbFrequencyRead_RX *rx);

#endif /* _KERNEL_MODULE_H_ */
