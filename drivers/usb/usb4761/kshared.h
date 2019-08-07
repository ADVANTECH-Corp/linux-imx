/*
 * kshared.h
 *
 *  Created on: 2013-6-15
 *      Author: 
 */

#ifndef KERNEL_MODULE_SHARED_H_
#define KERNEL_MODULE_SHARED_H_

#include <linux/types.h>


#define ADVANTECH_VID 0x1809
#define DEVICE_ID  	 0x4761
#define DEVICE_PID    BD_USB4761
#define DEVICE_NAME   "USB-4761"
#define DRIVER_NAME   "bio4761"

// ----------------------------------------------------------
// H/W feature and structures.
// ----------------------------------------------------------
#define DIO_PORT_COUNT           1  // 1 DI, 1 DO
#define DIO_CHL_COUNT            (DIO_PORT_COUNT * 8)
#define DI_INT_SRC_COUNT         8  // all DI channel
#define DI_SNAP_SRC_COUNT        DI_INT_SRC_COUNT

enum KRNL_EVENT_IDX{
   KdxDevPropChged = 0,
   KdxDiBegin,
   KdxDiintChan0 = KdxDiBegin,
   KdxDiintChan1,
   KdxDiintChan2,
   KdxDiintChan3,
   KdxDiintChan4,
   KdxDiintChan5,
   KdxDiintChan6,
   KdxDiintChan7,
   KdxDiEnd      = KdxDiintChan7,
   KrnlSptedEventCount,
};

static inline __u32 GetEventKIndex(__u32 eventType)
{
   __u32 kdx = -1;
   if (eventType == EvtPropertyChanged) {
      kdx = KdxDevPropChged;
   } else {
      eventType -= EvtDiintChannel000;
      if (eventType < DI_SNAP_SRC_COUNT) {
         kdx = KdxDiBegin + eventType;
      }
   }

   return kdx;
}

// -----------------------------------------------------
// default values
// -----------------------------------------------------
#define DEF_INIT_ON_LOAD        1

// DIO default values
#define DEF_DO_STATE            0
#define DEF_DI_INT_TRIGEDGE     RisingEdge

// ----------------------------------------------------------
// Device private data
// ----------------------------------------------------------
typedef struct _DI_SNAP_CONFIG
{
   __u8 PortStart;
   __u8 PortCount;
} DI_SNAP_CONFIG;

typedef struct _DI_SNAP_STATE
{
   __u8 State[DIO_PORT_COUNT];
} DI_SNAP_STATE;

typedef struct _DEVICE_SHARED
{
   __u32          Size;           // Size of the structure
   __u32          ProductId;      // Device Type
   __u32          DeviceNumber;   // Zero-based device number

   // HW Information
   __u32          BoardId;        // Board dip switch number for the device
   __u32          InitOnLoad;

   // --------------------------------------------------------
   __u8           DoPortState[DIO_PORT_COUNT];
   __u8           DiintTrigEdge[DI_INT_SRC_COUNT];
   DI_SNAP_CONFIG DiSnapParam[DI_SNAP_SRC_COUNT];
   DI_SNAP_STATE  DiSnapState[DI_SNAP_SRC_COUNT];

   // ---------------------------------------------------------
   __u32          IntCsr;
   __u32          IsEvtSignaled[KrnlSptedEventCount];

} DEVICE_SHARED;

#endif /* KERNEL_MODULE_SHARED_H_ */
