/*
 * kshared.h
 *
 *  Created on: August 1, 2012
 *      Author: rocky
 */

#ifndef KERNEL_MODULE_SHARED_H_
#define KERNEL_MODULE_SHARED_H_

#include <linux/types.h>


#define ADVANTECH_VID    0x1809
#define DEVICE_ID_4704   0x4704
#define DEVICE_ID_4702   0x4702
#define DRIVER_NAME     "bio4704"

#define DEVICE_NAME_FROM_PID(pid) \
   (pid == BD_USB4704 ? "USB-4704" : "USB-4702")
#define DEVICE_ID_FROM_PID(pid) \
   (pid == BD_USB4704 ? DEVICE_ID_4704 : DEVICE_ID_4702)

// ----------------------------------------------------------
// H/W feature and structures.
// ----------------------------------------------------------
#define AI_CHL_COUNT             8
#define AI_SE_CHL_COUNT          AI_CHL_COUNT
#define AI_DIFF_CHL_COUNT        (AI_CHL_COUNT / 2)

#define FAIINT_SPEED_UPLIMIT(ProductId) ( (ProductId == BD_USB4704) ? 47619 : 10000 ) // USB4702:10K Hz, USB4704: 48K Hz,
                                                                                      // and only FAI in running with no DIO¡¢AO is in working status
#define FAIINT_SPEED_DOWNLIMIT   32         // 32Hz (2M/65535=30.5)
#define AI_GAIN_COUNT            9 // The FW save position: Diff: 8, SIE: 1, total 9 gains

#define AI_MAX_PACER(ProductId)  ( (ProductId == BD_USB4704) ? (47619) : (10*1000) )// USB4702:10KHz, USB4704: 48KHz
#define AI_MAX_PACER_4704        (47619)
#define AI_MAX_PACER_4702        (10*1000)
#define AI_MIN_PACER             (FAIINT_SPEED_DOWNLIMIT)

#define AI_CLK                   2000000 //MCU SYSCLK frequency = 24000000, AI Clock = 24000000/12=2000000 

#define AI_CLK_BASE              (10*1000*1000)       // 10MHz clock
#define AI_FIFO_SIZE             512                 // Ai fifo size in samples.
#define AI_CHL_MASK              (AI_CHL_COUNT - 1)   //
#define AI_DATA_SIZE             sizeof(unsigned short)
#define AI_RES_IN_BIT(ProductId) ((ProductId == BD_USB4704) ? 14 : 12) // USB4702: 12bits, USB4704: 14bits
#define AI_DATA_MASK(ProductId)  ((ProductId == BD_USB4704) ? 0x3fff : 0xfff)
#define AI_RES_IN_BIT_4704       14
#define AI_RES_IN_BIT_4702       12
#define AI_DATA_MASK_4704        0x3fff
#define AI_DATA_MASK_4702        0xfff

#define AI_GAIN_V_Neg10To10      0  //SingleEnded

#define FAI_CONTINUOUS_READER_PACKET_SIZE     (4*1024)//64
#define FAI_CONTINUOUS_READER_PENDING_READS   2//4

//////////////////////////////////////////////////////////////////////////
//AO
#define AO_CHL_COUNT             2
#define AO_CHL_MASK              (AO_CHL_COUNT - 1)   // 0x1 for 2 channel AO.
#define AO_RES_IN_BIT            12
#define AO_DATA_SIZE             sizeof(__u16)
#define AO_DATA_MASK             0xfff

#define AO_GAIN_V_0To5           0

//////////////////////////////////////////////////////////////////////////
//DIO
#define DIO_PORT_COUNT           1
#define DIO_CHL_COUNT            (DIO_PORT_COUNT * 8)

//////////////////////////////////////////////////////////////////////////
// Counter
#define CNTR_CHL_COUNT           1
#define CNTR_RES_IN_BIT          32
#define CNTR_DATA_SIZE           sizeof(ULONG)
#define CNTR_IDLE                0

#define CNTR_RBUF_DEPTH          16                                       // using a 2^n to avoid using '%'(mod) calculation.
#define CNTR_RBUF_POSMASK        (CNTR_RBUF_DEPTH -1)                     // the mask of 'POS' of counter ring-buffer
#define CNTR_CHK_PERIOD_MS       5                                        // using a 5ms timer for overflow check and freq measure
#define CNTR_CHK_PERIOD_NS       ((__u32)CNTR_CHK_PERIOD_MS * 1000 * 10)  // check period in 100ns unit.
#define CNTR_CHK_PERIOD_MAX_NS   (((__u32)10 * 1000 * 1000 * 10) >> 4 )   // max period for checking counter status & value, in 100ns unit.

#define CNTR_VAL_THRESHOLD_BASE  10      // the threshold of counter value difference between two read, for frequency measurement mainly.

#define CNTR_CLK_BASE            (24*1000*1000)       // MHz clock

//////////////////////////////////////////////////////////////////////////
// Note: This device different with the other USB devices
#define SET_TRIGERMODE           0x3000
#define GET_TRIGERFLG            0x3001
#define SET_OSCILLATOR           0x3002
#define GET_OSCILLATOR           0x3003

#define DAn_OUT(n)               (0x4000 + n)
#define ADn_IN(n)                (0x5000 + n)

#define ADn_CALI_SET_Gm(n, m)    (0x5100 + m*8 + n)
#define ADn_CALI_GET_Gm(n, m)    (0x5148 + m*8 + n)

#define DAn_CALI_SET(n)          (0x5190 + n)
#define DAn_CALI_GET(n)          (0x5192 + n)

#define DI_IN                    0x6000
#define DO_OUT                   0x6001
#define DIO_RESET                0x6002 //not used
#define DO_STATUS_GET            0x6003

#define COUNTER_START            0x6100
#define COUNTER_READ             0x6101
#define COUNTER_CHECK            0x6102
#define COUNTER_RESET            0x6103 //not used
#define COUNTER_STOP             0x6104

#define FAI_MCHANNEL_GAIN_SET    0x7008 //not used
#define FAI_CHANNEL_PROPERY_SET  0x7009
#define FAI_DEV_PROPERY_SET      0x7010
#define FAI_CONVNUM_SET          0x7011
#define FAI_START                0x7012
#define FAI_STOP                 0x7013
#define FAI_GET_STATUS           0x7014 //not used


enum KRNL_EVENT_IDX{
   KdxDevPropChged = 0,
   KdxAiDataReady,
   KdxAiOverrun,
   KdxAiStopped,
   KdxAiCacheOverflow,
   KrnlSptedEventCount,
};

static inline __u32 GetEventKIndex(__u32 eventType)
{
   __u32 kdx;
   switch ( eventType )
   {
   case EvtPropertyChanged:         kdx = KdxDevPropChged;    break;
   case EvtBufferedAiDataReady:     kdx = KdxAiDataReady;     break;
   case EvtBufferedAiOverrun:       kdx = KdxAiOverrun;       break;
   case EvtBufferedAiStopped:       kdx = KdxAiStopped;       break;
   case EvtBufferedAiCacheOverflow: kdx = KdxAiCacheOverflow; break;
   default: kdx = -1; break;
   }
   return kdx;
}

// -----------------------------------------------------
// macros the daqusb.h needed
// -----------------------------------------------------
#define  AI_CHANNEL_CONFIG
#define  MAX_AI_CHANNELS        AI_CHL_COUNT
#define  MAX_COUNTER_PARA       3

// -----------------------------------------------------
// default values
// -----------------------------------------------------
#define DEF_INIT_ON_LOAD        1

// AI default values
#define DEF_AI_CHTYPE           0 // single-ended
#define DEF_AI_GAIN             AI_GAIN_V_Neg10To10   //SingleEnded
#define DEF_FAI_CHSTART         0
#define DEF_FAI_CHCOUNT         1
#define DEF_FAI_CLKSRC          SigInternalClock
#define DEF_FAI_PACERDIVISOR    (100 * 100)
#define DEF_FAI_SECTSIZE        (AI_FIFO_SIZE / 2)
#define DEF_FAI_MODE            0

// AO default values
#define DEF_AO_GAIN             AO_GAIN_V_0To5
#define DEF_AO_STATE            0

// DIO default values
#define DEF_DO_STATE            0

// Counter default values
#define DEF_CNT_PERIOD          CNTR_CHK_PERIOD_NS


// ----------------------------------------------------------
// Device private data
// ----------------------------------------------------------
typedef struct _AI_CALI_INFO {
   __u8 Offset;
   __u8 Span;
}AI_CALI_INFO;

typedef struct _FAI_CONFIG
{
   __u32  XferMode;
   __u32  PhyChanStart;
   __u32  LogChanCount;
   __u32  ConvClkSource;
   __u32 ConvClkRatePerCH;
   __u32  SectionSize;
   __u32  SampleCount;
} FAI_CONFIG;

typedef struct _FAI_STATUS
{
   __u32  AcqMode;
   __u32  FnState;
   __u32  BufState;
   __u32  BufLength;
   __u32  WPRunBack;
   __u32  WritePos;
   __u32  ReadPos;
   AI_CALI_INFO ChanCaliData[AI_CHL_COUNT];
   __u32  CurrChan;  // Current scanning channel, used for calibrate the data
   __u32  HwFlag;
}FAI_STATUS;

typedef struct _USB_TMR_CONFIG
{
   __u32  Context[3];
}USB_TMR_CONFIG;

typedef struct _CNTR_CONFIG
{
   __u32  FmPeroid[CNTR_CHL_COUNT];
}CNTR_CONFIG;

typedef struct _CNTR_STATE
{
   __u32        Operation;   // the operation the counter is running
   __u32        CurrValue;   // The current counter value, used for FM to retrieve data fro USB device Asyn
   __u32        AdaptivePeriod;     // the function used to check the counter status & value.

   // event counting running time state ------------------------------------------------
   __u32        Overflow;  // overflow count of the counter. for event count.

   // frequency measurement running time state------------------------------------------
   __u32        CheckPeriod;   // period to check the counter status & value, in 100ns unit.
   __u32        PrevVal;       // the previous counter value in raw format.
   __u32        SummedValue;   // accumulated counter value. for freq measurement mainly.
   __u32        PrevTime;      // the previous timestamp, in 100ns unit.
   __u64        TotalTime;     // elapsed time for counting up to the 'Summed' value, in 100ns unit.

   struct  {  // using a ring-buffer to accumulate the counter value
      __u32  Head;        // data position of the oldest data
      __u32  Tail;        // data position of the latest data.

      // for data alignment, these two field are defined separately.
      __u32  CntrDelta[CNTR_RBUF_DEPTH];  // the difference of counter value with the previous one.
      __u32  TimeDelta[CNTR_RBUF_DEPTH];  // the timestamp of the counter value, in 100ns unit.
   };
}CNTR_STATE;

typedef struct _DEVICE_SHARED
{
   __u32       Size;           // Size of the structure
   __u32       ProductId;      // Device Type
   __u32       DeviceNumber;   // Zero-based device number

   // HW Information
   __u32       BoardId;        // Board dip switch number for the device
   __u32       InitOnLoad;
   __u8        HwAiChType;
   __u8        Oscillator;        // This device must always use external Oscillator, the MCU internal Oscillator will cause data incorrect

   // --------------------------------------------------------
   __u8        AiChanType[AI_CHL_COUNT];
   __u8        AiChanGain[AI_CHL_COUNT];
   __u32       AiLogChanCount;
   FAI_CONFIG  FaiParam;
   FAI_STATUS  FaiStatus;

   // ---------------------------------------------------------
   __u32       AoChanGain[AO_CHL_COUNT];
   __u16       AoChanState[AO_CHL_COUNT];

   // ---------------------------------------------------------
   __u8        DoPortState[DIO_PORT_COUNT];

   // ---------------------------------------------------------
   CNTR_CONFIG CntrConfig;
   CNTR_STATE  CntrState[CNTR_CHL_COUNT];
   __u32       CntrChkTimerOwner;  // bit-mapped field indicates who owns the WDFTIMER for counter state checking.

   // ---------------------------------------------------------
   __u32       IsEvtSignaled[KrnlSptedEventCount];
} DEVICE_SHARED;

#endif /* KERNEL_MODULE_SHARED_H_ */
