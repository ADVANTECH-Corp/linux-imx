/*
 * hw.h
 *
 *  Created on: 2013-6-15
 *      Author: 
 */

#ifndef _KERNEL_MODULE_HW_H_
#define _KERNEL_MODULE_HW_H_

// interrupt control & status register
#define INT_DISABLED          0
#define INT_SRC_DI            1

#define TRIG_EDGE_RISING      1  // rising edge. note: this is the device specified value.
#define TRIG_EDGE_FALLING     0  // falling edge. note: this is the device specified value.

typedef struct _EE_READ
{
   __u32 Data;
   __u32 LastError;
}EE_READ,*PEE_READ;

typedef struct _EE_WRITE
{
   __u32 Data;
   __u16 Addr;
}EE_WRITE;

#define MAX_EVT_NUM_IN_FIFO 256
typedef struct _EVENT_DATA
{
   __u8 PortData;
   __u8 EventType;
}EVENT_DATA;

typedef struct _FW_EVENT_FIFO
{
   __u8       EventCount;
   EVENT_DATA EventData[MAX_EVT_NUM_IN_FIFO];
}FW_EVENT_FIFO;

#endif /* _KERNEL_MODULE_HW_H_ */
