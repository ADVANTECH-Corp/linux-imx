/*
 * Ioctls.h
 *
 *  Created on: 2011-8-29
 *      Author: rocky
 */

#ifndef BIONIC_DAQ_IOCTLS_DEF_H_
#define BIONIC_DAQ_IOCTLS_DEF_H_

#include <linux/ioctl.h>

// Magic Number of IOCTLs
#define BDAQ_DEV_MAGIC  'a'
#define BDAQ_AI_MAGIC   'b'
#define BDAQ_AO_MAGIC   'c'
#define BDAQ_DIO_MAGIC  'd'
#define BDAQ_CNTR_MAGIC 'e'
#define BDAQ_KLIB_MAGIC 'k'

// To compatible with 32bit and 64bit OS at same time,
// pointer(handle) type field of a structure should be declared like this:
// Compatible(<type>, <name>)
#define DECL_64COMPAT(type, name)      union { type name; int64 _pad##name; }

//******************************************************************************************
//                                                                                         *
// IOCTL for klib operation                                                                *
//                                                                                         *
//******************************************************************************************
// Input: <None>
// Output: Handle of event
#define IOCTL_KLIB_CREATE_EVENT               _IO(BDAQ_KLIB_MAGIC, 1)

// Input: Handle of event
// Output: <None>
#define IOCTL_KLIB_CLOSE_EVENT                _IO(BDAQ_KLIB_MAGIC, 2)

// Input: Handle of event
// Output: <None>
#define IOCTL_KLIB_SET_EVENT                  _IO(BDAQ_KLIB_MAGIC, 3)

// Input: Handle of event
// Output: <None>
#define IOCTL_KLIB_RESET_EVENT                _IO(BDAQ_KLIB_MAGIC, 4)

// Input: pointer to structure DIO_RW_PORTS.
// Output: <None>
typedef struct _KLIB_WAIT_EVENTS{
   int32  Result;
   int32  Timeout;
   int32  WaitAll;
   uint32 Count;
   HANDLE *Events;
}KLIB_WAIT_EVENTS;

#define IOCTL_KLIB_WAIT_EVENTS                _IO(BDAQ_KLIB_MAGIC, 5)

#ifndef ISA_NAME_MAX_LEN
#define ISA_NAME_MAX_LEN  32
#endif
// Input: pointer of structure ISA_DEV_INFO
// Output: <None>
typedef struct _ISA_DEV_INFO{
   char driver_name[ISA_NAME_MAX_LEN];
   char device_name[ISA_NAME_MAX_LEN];
   uint32 iobase;
   uint32 irq;
   uint32 dmachan;
}ISA_DEV_INFO;

// Input:
// Output:
#define IOCTL_KLIB_ADD_DEV                   _IO(BDAQ_KLIB_MAGIC, 6)
#define IOCTL_KLIB_REMOVE_DEV                _IO(BDAQ_KLIB_MAGIC, 7)

//******************************************************************************************
//                                                                                         *
// IOCTL for device operation                                                              *
//                                                                                         *
//******************************************************************************************
// Input:  <None>
// Output: CHAR[64], device description
#define IOCTL_DEVICE_GET_DESC                _IO(BDAQ_DEV_MAGIC, 1)

// Input: <None>
// Output: CHAR[64], board version(firmware version)
#define IOCTL_DEVICE_GET_BOARD_VER           _IO(BDAQ_DEV_MAGIC, 2)

// Input: <None>
// Output: <None>
#define IOCTL_DEVICE_LOCATE_DEVICE           _IO(BDAQ_DEV_MAGIC, 3)

// Input: <None>
// Output: CHAR[64], device location information
#define IOCTL_DEVICE_GET_LOCATION_INFO       _IO(BDAQ_DEV_MAGIC, 4)

// Input: the uint32 type boardID to be set to device
// Output: <None>
// Add these ioctrl code to manage the boardID problem of USB device.
#define IOCTL_DEVICE_SET_BOARDID             _IO(BDAQ_DEV_MAGIC, 5)

/*------------------------------------------------------------------*/
// input:  an array of struct USER_EVENT_INFO.
// output: <none>
typedef struct _USER_EVENT_INFO{
   uint32  EventId;
   HANDLE  Handle;
}USER_EVENT_INFO;

#define IOCTL_DEVICE_REGISTER_USER_EVENT       _IO(BDAQ_DEV_MAGIC, 10)
#define IOCTL_DEVICE_UNREGISTER_USER_EVENT     _IO(BDAQ_DEV_MAGIC, 11)

// Input: pointer of EVENT_GET_STATUS
// Output: status in EVENT_GET_STATUS
typedef struct _EVENT_GET_STATUS{
   uint32 LParam;
   uint32 RParam;
}EVENT_GET_STATUS;

#define IOCTL_DEVICE_GET_EVENT_LAST_STATUS     _IO(BDAQ_DEV_MAGIC, 12)


// Input: pointer of structure EVENT_CLEAR_FLAG
// Output: <None>
typedef struct _EVENT_CLEAR_FLAG{
   uint32 EventId;
   uint32 LParam;
   uint32 RParam;
}EVENT_CLEAR_FLAG;

#define IOCTL_DEVICE_CLEAR_EVENT_FLAG          _IO(BDAQ_DEV_MAGIC, 13)

// Input:
// Output:
#define IOCTL_DEVICE_NOTIFY_PROP_CHGED         _IO(BDAQ_DEV_MAGIC, 14)

// Input: event id
// Output:
#define IOCTL_DEVICE_TRIGGER_EVENT             _IO(BDAQ_DEV_MAGIC, 15)

/*------------------------------------------------------------------*/
// -------------------------------------------------------------------
// Special!
// -------------------------------------------------------------------
// For debug and calibration only!!!!!!!!!!!!!!!!!!
typedef struct _DBG_REG_IO{
   uint64 Addr;
   uint32 Length;
   void   *Data;
}DBG_REG_IO;

#define IOCTL_DEVICE_DBG_REG_IN              _IO(BDAQ_DEV_MAGIC, 21)
#define IOCTL_DEVICE_DBG_REG_OUT             _IO(BDAQ_DEV_MAGIC, 22)

// For debug and calibration only!!!!!!!!!!!!!!!!!!
typedef struct _DBG_USB_IO{
   uint16  MajorCmd;
   uint16  MinorCmd;
   uint32  Length;
   void    *Data;
}DBG_USB_IO;

#define IOCTL_DEVICE_DBG_USB_IN              _IO(BDAQ_DEV_MAGIC, 23)
#define IOCTL_DEVICE_DBG_USB_OUT             _IO(BDAQ_DEV_MAGIC, 24)

#define IOCTL_DEVICE_MAKE_SHARED             _IO(BDAQ_DEV_MAGIC, 25)

#define DRVSPEC_DN3  3 
#define DRVSPEC_DN4  4 
typedef struct _DEVICE_HWINFO {
   uint32 DeviceNumber;  // -1 if it is DN4 driver
   uint32 ProductId;
   uint32 BoardId;
   union {
      uint32 BusSlot;    
      uint32 IoBase;     
   };
   uint32 DriverSpec;    
   char   DriverName[32];
   char   DeviceName[32];
} DEVICE_HWINFO, *PDEVICE_HWINFO;

#define IOCTL_DEVICE_GET_HWINFO              _IO(BDAQ_DEV_MAGIC, 26)

//******************************************************************************************
//                                                                                         *
// IOCTL for analog input operation                                                        *
//                                                                                         *
//******************************************************************************************
// Input:    AI_SET_CHAN
// Output:   <none>
#define AI_SET_CHSCTYPE  0x1
#define AI_SET_CHGAIN    0x2
#define AI_SET_CH_ALL     (-1)

typedef struct _AI_CHAN_CFG{
   uint8  SCType;  // single-ended or differential
   uint8  Gain;    // gain code of the channel.
}AI_CHAN_CFG;

typedef struct _AI_SET_CHAN{
   uint32      SetWhich;      // set which
   uint32      PhyChanStart;  // Physical channel number of the start channel to configure.
   uint32      PhyChanCount;  // Physical channel count to configure.
   AI_CHAN_CFG *ChanCfg;      // variable length array for channel data.
}AI_SET_CHAN;

#define IOCTL_AI_SET_CHAN_CFG                _IO(BDAQ_AI_MAGIC, 1)

// Input:   AI_SCAN_CHL.
// Output:
//    an array of Binary data. The minimize size of the output buffer is
//    channel count to scan multiplies the size of each data.
typedef struct _AI_READ_SAMPLES{
   uint32 PhyChanStart; // Physical channel number of the start channel
   uint32 LogChanCount; // logical channel count
   void   *Data;
}AI_READ_SAMPLES;

#define IOCTL_AI_READ_SAMPLES                _IO(BDAQ_AI_MAGIC, 2)

// Input:  AI_READ_CJC_SAMPLES.
// Output: an array of Binary data.
typedef struct _AI_READ_CJC_SAMPLES{
   uint32 CjcError;    //CJC error
   uint32 CjcValCount; //The count of CJC Value
   void   *CjcValues;  //variable length array for CJC value
}AI_READ_CJC_SAMPLES;

#define IOCTL_AI_READ_CJC_SAMPLES            _IO(BDAQ_AI_MAGIC, 3)

// Get the channel's status of USB device
//    used for Burn Detect and calibration
#define IOCTL_AI_BURN_DETECT                 _IO(BDAQ_AI_MAGIC, 4)

// Input: pointer to structure FAI_PARAM.
// Output: <None>
#define IOCTL_FAI_SET_PARAM                  _IO(BDAQ_AI_MAGIC, 5)

// Input: pointer to user buffer
// Output: <None>
#define IOCTL_FAI_SET_BUFFER                 _IO(BDAQ_AI_MAGIC, 6)

// Input:  __u32, acquisition mode: FINITE or INFINITE
// Output: <None>
#define IOCTL_FAI_START                      _IO(BDAQ_AI_MAGIC, 7)

// Input: __u32, other flags.
// Output: <None>
#define FAI_STOP_FREE_RES  0x1   // free resource or not: 0 -- do not free resource, 1 -- free resource.
#define IOCTL_FAI_STOP                       _IO(BDAQ_AI_MAGIC, 8)

//******************************************************************************************
//                                                                                         *
// IOCTL for analog output operation                                                       *
//                                                                                         *
//******************************************************************************************
#define AO_SET_CHVRG      0x1
#define AO_SET_EXTREFUNI  0x2
#define AO_SET_EXTREFBI   0x4
#define AO_SET_ALL        -1
typedef struct _AO_SET_CHAN{
   uint32  SetWhich;
   double  ExtRefUnipolar;        // unipolar external reference value
   double  ExtRefBipolar;         // bipolar external reference value
   uint32  ChanStart;             // channel number of the start channel to configure.
   uint32  ChanCount;             // channel number of the end channel to configure.
   uint32  *Gains;                // variable length array for channels gain.
}AO_SET_CHAN;

#define IOCTL_AO_SET_CHAN_CFG                _IO(BDAQ_AO_MAGIC, 1)

// Input:  AO_WRITE_SAMPLES.
// Output: <None>
typedef struct _AO_WRITE_SAMPLES{
   uint32 ChanStart;     // channel number of the start channel to write data to.
   uint32 ChanCount;     // count of channel to write data to.
   void   *Data;         // variable length array for channel data. the array length
                        // should match the channel count.
}AO_WRITE_SAMPLES;

#define IOCTL_AO_WRITE_SAMPLES               _IO(BDAQ_AO_MAGIC, 2)

// Input:  pointer to structure FAO_FUNC_CFG.
// Output: <None>
#define IOCTL_FAO_SET_PARAM                  _IO(BDAQ_AO_MAGIC, 3)

// Input: pointer to user buffer
// Output: <None>
#define IOCTL_FAO_SET_BUFFER                 _IO(BDAQ_AO_MAGIC, 4)

// Input: uint32 : acquisition mode: FINITE or INFINITE
// Output buffer: <None>
#define IOCTL_FAO_START                      _IO(BDAQ_AO_MAGIC, 5)

// Input: uint32, other flags.
// Output: <None>
#define FAO_STOP_FREE_RES  0x1   // free resource or not: 0 -- do not free resource, 1 -- free resource.
#define FAO_STOP_BREAK_NOW 0x2   // wait for the working buffer empty or not: 0 -- wait, 1 -- no wait, stop immediately,
#define IOCTL_FAO_STOP                       _IO(BDAQ_AO_MAGIC, 6)

//******************************************************************************************
//                                                                                         *
// IOCTL for digital input/output operation                                                *
//                                                                                         *
//******************************************************************************************

// Input:  pointer to structure DIO_SET_PORT_DIR.
// Output: <None>
typedef struct _DIO_SET_PORT_DIR {
   uint32 PortStart;           // start port to configure
   uint32 PortCount;           // ports count to configure
   uint8  *Dirs;               // array for port direction.
} DIO_SET_PORT_DIR;

#define IOCTL_DIO_SET_PORT_DIR               _IO(BDAQ_DIO_MAGIC, 1)

// Input:   pointer to structure DIO_SET_DIINT_CFG.
// Output:  <None>
#define DIINT_SET_TRIGEDGE     0x1
#define DIINT_SET_RISING_EDGE  0x2
#define DIINT_SET_FALLING_EDGE 0x3
#define DIINT_SET_GATECTRL     0xF
typedef struct _DIO_SET_DIINT_CFG {
   uint32 SetWhich;
   uint32 SrcStart;             // start DI INT source to configure
   uint32 SrcCount;             // DI INT source count to configure
   uint8  *Buffer;              // array for DI interrupt trigger edge / gate control.
} DIO_SET_DIINT_CFG;

#define IOCTL_DIO_SET_DIINT_CFG               _IO(BDAQ_DIO_MAGIC, 2)

// Input:  pointer to structure DIO_SET_DICOS_CFG
// Output: <None>
typedef struct _DIO_SET_DICOS_CFG {
   uint32 SrcStart;      // start DI CoS source to configure
   uint32 SrcCount;      // DI CoS source count to configure
   uint8  *ChanEnable;   // each bit specified whether enable/disable the for a DI channel. 0 -- disable, 1 -- enable.
} DIO_SET_DICOS_CFG;

#define IOCTL_DIO_SET_DICOS_CFG                _IO(BDAQ_DIO_MAGIC, 3)

// Input:  pointer to structure DIO_SET_DIPM.
// Output: <None>
#define DIPM_SET_PORTVAL  0x1
#define DIPM_SET_CHENBED  0xF
typedef struct _DIO_SET_DIPM_CFG {
   uint32 SetWhich;    // set which parameter
   uint32 SrcStart;    // start DI Pm source to configure
   uint32 SrcCount;    // DI Pm source count to configure
   uint8  *Buffer;  // variable length array: Port value or channel enabled map.
} DIO_SET_DIPM_CFG;

#define IOCTL_DIO_SET_DIPM_CFG                 _IO(BDAQ_DIO_MAGIC, 4)

// Input:  pointer to structure DIO_SET_DIINT_GATECTRL.
// Output: <None>
#define DIFLT_SET_BLKINTVL 0x1
#define DIFLT_SET_CHENBED  0x2
#define DIFLT_SET_ALL      -1
typedef struct _DIO_SET_DIFLT_CFG {
   uint32 SetWhich;
   uint32 BlkTime;          // di filter block interval value, in device unit.
   uint32 SrcStart;         // start port to configure
   uint32 SrcCount;         // port count to configure
   uint8  *ChanEnabled;     // each bit specified whether enable/disable the for a DI channel. 0 -- disable, 1 -- enable.
} DIO_SET_DIFLT_CFG;

#define IOCTL_DIO_SET_DIFLT_CFG                _IO(BDAQ_DIO_MAGIC, 5)

// Input: pointer to structure DIO_RW_PORTS.
// Output: DI port data, stored in DIO_RW_PORTS.PortData[].
typedef struct _DIO_RW_PORTS {
   uint32 PortStart;             // start port to configure
   uint32 PortCount;             // ports count to configure
   uint8  *Data;                 // variable length array of DO ports data to output.
} DIO_RW_PORTS;

#define IOCTL_DIO_READ_DI_PORTS                _IO(BDAQ_DIO_MAGIC, 6)

// Input: pointer to structure DIO_RW_PORTS.
// Output: <None>.
#define IOCTL_DIO_WRITE_DO_PORTS               _IO(BDAQ_DIO_MAGIC, 7)

// Input: pointer to structure DIO_RW_PORTS.
// Output: DO port data, stored in DIO_RW_PORTS.PortData[].
#define IOCTL_DIO_READ_DO_PORTS                _IO(BDAQ_DIO_MAGIC, 8)

// Input:   pointer to structure DIO_START_DI_SNAP.
// Output:  <none>
typedef struct _DIO_START_DI_SNAP{
   uint32 EventId;               // event type to trigger DI snap function
   uint32 PortStart;             // start DI port to scan when the event occurred.
   uint32 PortCount;             // ports count to scan when the event occurred.
} DIO_START_DI_SNAP;

#define IOCTL_DIO_START_DI_SNAP                _IO(BDAQ_DIO_MAGIC, 9)

// Input: uint32, the event type for which to stop the DI snap function.
// Output: <none>
#define IOCTL_DIO_STOP_DI_SNAP                 _IO(BDAQ_DIO_MAGIC, 10)

// Input:  uint32, 0 -- disable the function, 1 -- enable the function
// Output: <none>
#define IOCTL_DIO_ENABLE_DO_FREEZE             _IO(BDAQ_DIO_MAGIC, 11)

// Input:  <none>
// Output: uint32, freeze state: 0 -- the do channels are not frozen, 1 -- the do channels are frozen
#define IOCTL_DIO_POLL_DO_FREEZE_STATE         _IO(BDAQ_DIO_MAGIC, 12)

// Input:  uint8[], dio ports power on direction
// Output: <None>
#define IOCTL_DIO_SET_POWER_ON_DIR             _IO(BDAQ_DIO_MAGIC, 13)

// Input:  uint8[], do ports power on state
// Output: <None>
#define IOCTL_DIO_SET_DO_POWER_ON_STATE        _IO(BDAQ_DIO_MAGIC, 14)

// Input:  pointer to structure DIO_SET_DOWTD_CFG.
// Output: <None>
#define WDT_SET_INTERVAL      0x1
#define WDT_SET_LOCKVALUE     0x2
#define WDT_SET_ALL            -1
typedef struct _DIO_SET_WDT_CFG {
   uint32 SetWhich;
   uint32 Interval;
   uint32 SrcStart;         // start port to configure
   uint32 SrcCount;         // port count to configure
   uint8  *LockValue;       // variable length array, each element stands for one DO port's locked state
} DIO_SET_WDT_CFG;

#define IOCTL_DIO_SET_DOWDT_CFG                _IO(BDAQ_DIO_MAGIC, 15)

// Input:  uint32, the command to execute
// Output: <None>
#define WDT_CMD_START   0x0
#define WDT_CMD_STOP    0x1
#define WDT_CMD_FEED    0x2
#define IOCTL_DIO_EXEC_WDT_CMD                _IO(BDAQ_DIO_MAGIC, 16)

// Input: pointer to structure DIO_RW_BIT.
typedef struct _DIO_RW_BIT {
   uint32 Port;            // port to configure
   uint32 Bit;             // count to configure
   uint8  Data;            // variable of DO bit data to output.
} DIO_RW_BIT;
#define IOCTL_DIO_WRITE_DO_BIT                _IO(BDAQ_DIO_MAGIC, 17)
//******************************************************************************************
//                                                                                         *
// IOCTL for Counter/Timer operation                                                       *
//                                                                                         *
//******************************************************************************************
//

// Input: pointer to structure CNTR_SET_CFG
// Output: <None>
typedef struct _CNTR_SET_CFG{
   uint32 PropID;
   uint32 Start;
   uint32 Count;
   void   *Value;
}CNTR_SET_CFG;

#define IOCTL_CNTR_SET_CFG                     _IO(BDAQ_CNTR_MAGIC, 1)

// Input:  pointer to structure CNTR_START
// Output:  <None>
typedef struct _CNTR_START{
   uint32 Operation;
   uint32 Start;
   uint32 Count;
   void   *Param;
}CNTR_START;

#define IOCTL_CNTR_START                       _IO(BDAQ_CNTR_MAGIC, 2)

// Input:  pointer to structure CNTR_READ
// Output: depending on device and the operation value.
typedef struct _CNTR_READ{
   uint32 Start;
   uint32 Count;
   uint32 Operation;
   void   *Value;
}CNTR_READ;

typedef struct _CNTR_VALUE{
   uint16 CanRead;
   uint16 Overflow;
   uint32 Value;
}CNTR_VALUE;

#define IOCTL_CNTR_READ                        _IO(BDAQ_CNTR_MAGIC, 3)

// Input:  pointer to structure CNTR_RESET
// Output: <NONE>
typedef struct _CNTR_RESET{
   uint32 Start;
   uint32 Count;
}CNTR_RESET;

#define IOCTL_CNTR_RESET                       _IO(BDAQ_CNTR_MAGIC, 4)


// Input buffer:  pointer to structure CNTR_START_SNAP.
// Output buffer: <none>
typedef struct _CNTR_START_SNAP {
   uint32 EventType;             // event type to trigger DI snap function
   uint32 CntrStart;             // start DI port to scan when the event occurred.
   uint32 CntrCount;             // ports count to scan when the event occurred.
} CNTR_START_SNAP, *PCNTR_START_SNAP;

#define IOCTL_CNTR_START_SNAP                  _IO(BDAQ_CNTR_MAGIC, 5)


// Input buffer:  uint32, the event type for which to stop the DI snap function.
// Output buffer:  <none>
#define IOCTL_CNTR_STOP_SNAP                  _IO(BDAQ_CNTR_MAGIC, 6)

// Input buffer:  CNTR_CONTCMP_CFG
// Output buffer: <NONE>
#define CNTR_CONTCMP_TABLE  1
#define CNTR_CONTCMP_INTVL  2
typedef struct _CNTR_CONTCMP_CFG {
   uint32 Counter;
   uint32 Type;
   union {
      struct {
         uint32 Start;
         int32  Increment;
         uint32 Count;
      };
      struct {
         uint32 TblSize;
         uint32 *TblData;
      };
   };
} CNTR_CONTCMP_CFG, *PCNTR_CONTCMP_CFG;

#define IOCTL_CNTR_CONTCMP_CFG                  _IO(BDAQ_CNTR_MAGIC, 7)


// Input buffer:  uint32, the counter
// Output buffer: <NONE>
#define IOCTL_CNTR_CONTCMP_CLEAR                _IO(BDAQ_CNTR_MAGIC, 8)

// Input buffer:  CNTR_RESET
// Output buffer: <NONE>
#define IOCTL_CNTR_RESET_VALUE                  _IO(BDAQ_CNTR_MAGIC, 9)

#endif /* BIONIC_DAQ_IOCTLS_DEF_H_ */
