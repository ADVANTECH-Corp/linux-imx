/*******************************************************************************
              Copyright (c) 1983-2009 Advantech Co., Ltd.
********************************************************************************
THIS IS AN UNPUBLISHED WORK CONTAINING CONFIDENTIAL AND PROPRIETARY INFORMATION
WHICH IS THE PROPERTY OF ADVANTECH CORP., ANY DISCLOSURE, USE, OR REPRODUCTION,
WITHOUT WRITTEN AUTHORIZATION FROM ADVANTECH CORP., IS STRICTLY PROHIBITED. 

================================================================================
REVISION HISTORY
--------------------------------------------------------------------------------
$Log: $
--------------------------------------------------------------------------------
$NoKeywords:  $
*/

#ifndef BIONIC_KERNEL_MODE_LIB
#define BIONIC_KERNEL_MODE_LIB

#include <linux/version.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
#  error "This driver needs kernel version >= 2.6.0."
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
#include <linux/config.h>
#endif

// -------------------------------------------------------------------------------
// debug helper
// -------------------------------------------------------------------------------
#if defined(_DEBUG) || defined(DAQ_TRACING)
#  define daq_trace( _x_ )  printk _x_
#else
#  define daq_trace( _x_ )  do{}while(0)
#endif

// -------------------------------------------------------------------------------
// device id and sysfs methods & macros
// -------------------------------------------------------------------------------
dev_t daq_devid_alloc(void);
void  daq_devid_free(dev_t id);
struct class * daq_class_get(void);
struct device * daq_parent_get(void);

// -------------------------------------------------------------------------------
// event methods
// -------------------------------------------------------------------------------
#define MAX_WAIT_OBJECTS  64
typedef struct daq_event daq_event_t;

daq_event_t* daq_event_create(void);
void daq_event_close(daq_event_t* event);
void daq_event_set(daq_event_t* event);
void daq_event_reset(daq_event_t* event);
int  daq_event_wait(unsigned count, daq_event_t* events[], int waitAll, long timeout);

// -------------------------------------------------------------------------------
// memory.c
// -------------------------------------------------------------------------------
typedef struct _daq_umem {
   unsigned    nr_pages;
   struct page **pages;
   void        *kaddr;
} daq_umem_t;

int  daq_umem_map(unsigned long uaddr, unsigned count, int write, daq_umem_t *mi);
int  daq_umem_get_pages(unsigned long uaddr, unsigned count, int write, daq_umem_t *mi);
int  daq_umem_map_pages(daq_umem_t *mi);
void daq_umem_unmap(daq_umem_t *mi);
int  daq_umem_alloc(unsigned size, daq_umem_t *mi, int dma32);
void daq_umem_free(daq_umem_t *mi);

typedef struct _daq_dmem {
   unsigned size;
   void    *kaddr;
   __u64    daddr;
} daq_dmem_t;

int  daq_dmem_alloc(struct device *dev, unsigned size, daq_dmem_t *dmem);
void daq_dmem_free(struct device *dev, daq_dmem_t *dmem);

// -------------------------------------------------------------------------------
// misc.c
// -------------------------------------------------------------------------------
unsigned daq_ai_calc_log_chan_count(unsigned char * chType, unsigned count);
unsigned daq_ai_calc_phy_chan_range(unsigned char * chType, unsigned count, unsigned phyStart, unsigned logCount);
static inline unsigned daq_ai_calc_phy_chan_range32(unsigned char * chType, unsigned count, unsigned phyStart, unsigned logCount)
{
   unsigned range = daq_ai_calc_phy_chan_range(chType, count, phyStart, logCount);
   return ((range & 0xff00) << 8) | (range & 0x00ff);
}

typedef struct _daq_plx_sgl {
   unsigned              nr_pages;
   unsigned long         *pages;
   struct _PLX_SGL_ENTRY *start;
   struct _PLX_SGL_ENTRY *end;
   unsigned              count;
   unsigned              startPA;
} daq_plx_sgl_t;

int      daq_plx9054_flush_fifo(unsigned bridge_base, int dma_chan);
unsigned daq_plx905x_calc_sgl_length(unsigned data_len, unsigned sect_len);
int      daq_plx905x_alloc_sgl_mem(unsigned sgl_len, daq_plx_sgl_t *sgl_mem);
void     daq_plx905x_free_sgl_mem(daq_plx_sgl_t *sgl_mem);
int      daq_plx905x_build_sgl(
            daq_umem_t *data_mem, unsigned data_len,    unsigned  sect_len,
            unsigned   dev_addr,  unsigned from_device, daq_plx_sgl_t *sgl_mem);

unsigned daq_ver_string_to_integer(char * ver_str);

// -------------------------------------------------------------------------------
// USB continuous reader methods.
// -------------------------------------------------------------------------------
#if 1
//#ifdef CONFIG_USB   //disbale this for debain

#include <linux/usb.h>

#define MAX_PENDING_PACKET_COUNT       4
#define USB_EP_DIR(epd)                (epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
#define USB_EP_XFER_TYPE(epd)          (epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)

//
// return value of the call back function :
// DAQ_UR_CONTINUE : submit the URB again
// DAQ_UR_BREAK : don't submit the URB any more.
#define DAQ_UR_CONTINUE 0
#define DAQ_UR_BREAK    1
typedef int (*daq_usb_complete_t)(struct urb *urb, void *context);

#define is_reader_inited(reader) ((reader)->urb_count)
#define is_reader_running(reader) ((reader)->running)
typedef struct _daq_usb_reader{
   daq_usb_complete_t complete;
   void *context;
   int  running;
   int  urb_count;
   struct urb *urbs[MAX_PENDING_PACKET_COUNT];
}daq_usb_reader_t;

int daq_usb_reader_init(
      struct usb_device *udev,       const struct usb_endpoint_descriptor *epd,
      unsigned int      packet_size, unsigned int packet_count,
      daq_usb_complete_t on_read_complete, void *context, daq_usb_reader_t *reader);

int  daq_usb_reader_start(daq_usb_reader_t *reader);
void daq_usb_reader_stop(daq_usb_reader_t *reader);
void daq_usb_reader_cleanup(daq_usb_reader_t *reader);

#endif // CONFIG_USB

// -------------------------------------------------------------------------------
// ISA device.
// -------------------------------------------------------------------------------
extern struct list_head daq_isa_drivers;
struct daq_isa_device;

#define ISA_NAME_MAX_LEN         32

struct daq_isa_driver {
   struct list_head  isa_drivers;
   struct list_head  devices;
   const char *name;
   int (*probe)(struct daq_isa_device *dev);
   int (*remove)(struct daq_isa_device *dev);
   int (*match)(struct daq_isa_device *dev, struct daq_isa_driver * driver);
   int (*shutdown)(struct daq_isa_device *dev);
   int (*suspend)(struct daq_isa_device *dev);
   int (*resume)(struct daq_isa_device *dev);
};

struct daq_isa_device{
   struct list_head device_list;
   struct daq_isa_driver   *driver;
   const char device_name[ISA_NAME_MAX_LEN];
   const char driver_name[ISA_NAME_MAX_LEN];
   unsigned int iobase;
   unsigned int irq;
   unsigned int dmachan;
   struct  device dev;
};

int daq_isa_register_driver(struct daq_isa_driver *driver);
int daq_isa_unregister_driver(struct daq_isa_driver *driver);
int daq_isa_add_device(struct daq_isa_driver *driver, struct daq_isa_device *device);
int daq_isa_renove_device(struct daq_isa_driver *driver, struct daq_isa_device *device);
int daq_isa_match_obe_device(struct daq_isa_device *devuce, struct daq_isa_driver *driver);
int daq_add_isa_device(char *device_name, char * driver_name, unsigned int iobase, unsigned int irq, unsigned int dmabase);
int daq_remove_isa_device(char *device_name, char * driver_name, unsigned int iobase);

// -------------------------------------------------------------------------------
// MACROs to simplify read/write device register
// -------------------------------------------------------------------------------
// Access I/O Space
#define AdxIoInB(io_base, offset)           inb((io_base) + (offset))
#define AdxIoInW(io_base, offset)           inw((io_base) + (offset))
#define AdxIoInD(io_base, offset)           inl((io_base) + (offset))
#define AdxIoOutB(io_base, offset, data)    outb(( __u8)(data), (io_base) + (offset))
#define AdxIoOutW(io_base, offset, data)    outw((__u16)(data), (io_base) + (offset))
#define AdxIoOutD(io_base, offset, data)    outl((__u32)(data), (io_base) + (offset))

// Access Memory Space
#define AdxMemInB(mem_base, offset)           ioread8((mem_base) + (offset))
#define AdxMemInW(mem_base, offset)           ioread16((mem_base) + (offset))
#define AdxMemInD(mem_base, offset)           ioread32((mem_base) + (offset))
#define AdxMemOutB(mem_base, offset, data)    iowrite8 (( __u8)(data), (__u8*)(mem_base) + (offset))
#define AdxMemOutW(mem_base, offset, data)    iowrite16((__u16)(data), (__u8*)(mem_base) + (offset))
#define AdxMemOutD(mem_base, offset, data)    iowrite32((__u32)(data), (__u8*)(mem_base) + (offset))

// -------------------------------------------------------------------------------
// MACROs for compatible with different kernel version
// -------------------------------------------------------------------------------
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
#  define delayed_work                        work_struct
#  define delayed_work_ptr(_work)             (_work)
#  define DAQ_INIT_WORK(_work, _func)         INIT_WORK(_work, (void (*)(void *))_func, _work)
#  define DAQ_INIT_DELAYED_WORK(_work, _func) INIT_WORK(_work, (void (*)(void *))_func, _work)
#else
#  define delayed_work_ptr(_work)             container_of(_work, struct delayed_work, work)
#  define DAQ_INIT_WORK(_work, _func)         INIT_WORK(_work, _func)
#  define DAQ_INIT_DELAYED_WORK(_work, _func) INIT_DELAYED_WORK(_work, _func)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34))
#  define usb_alloc_coherent  usb_buffer_alloc
#  define usb_free_coherent   usb_buffer_free
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
#  ifdef CONFIG_PCI
#     include <linux/signal.h>
#     define IRQF_SHARED  SA_SHIRQ
#  endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
   typedef irqreturn_t (*daq_irq_handler_t)(int, void *, struct pt_regs *);
#else
#     include <linux/irqreturn.h>
   typedef irqreturn_t (*daq_irq_handler_t)(int, void *);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#     define time_is_before_jiffies(a) time_after(jiffies, a)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0))
#     define __devinit 
#     define __devexit
#     define __devexit_p(a)     a
#endif

#ifndef VM_RESERVED
#     define VM_RESERVED  (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#endif
