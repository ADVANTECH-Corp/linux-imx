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

#ifndef _ADVANTECH_DAQ_USB_PROTOCOL_DEFINE
#define _ADVANTECH_DAQ_USB_PROTOCOL_DEFINE

///////////////////////////////////////////////////////////////////////////////
// Error code
///////////////////////////////////////////////////////////////////////////////
#define BioUsbError 500
#define UsbError(n) (BioUsbError + n)

#define BioUsbTransmitFailed        UsbError(1)
#define BioUsbInvalidControlCode    UsbError(2)
#define BioUsbInvalidDataSize       UsbError(3)
#define BioUsbAIChannelBusy         UsbError(4)
#define BioUsbAIDataNotReady        UsbError(5)
#define BioUsbFirmwareUpdateFailed  UsbError(6)
#define BioUsbDeviceNotReady        UsbError(7)
#define BioUsbOperationNotSuccess   UsbError(8)

///////////////////////////////////////////////////////////////////////////////
// Control code for USB request
///////////////////////////////////////////////////////////////////////////////
#define MAJOR_DIO                0x01  
   #define MINOR_DO                    0x0001
   #define MINOR_DO_READ_TX            0x0002
   #define MINOR_DO_READ_RX            0x0003
   #define MINOR_DI_TX                 0x0004
   #define MINOR_DI_RX                 0x0005
   #define MINOR_MDO                   0x0006
   #define MINOR_MDO_READ_TX           0x0007
   #define MINOR_MDO_READ_RX           0x0008
   #define MINOR_MDI_TX                0x0009
   #define MINOR_MDI_RX	               0x000a

#define MAJOR_AI                 0x02
   #define MINOR_AI_SETGAIN            0x0001
   #define MINOR_AI_BINARYIN_TX        0x0002
   #define MINOR_AI_BINARYIN_RX        0x0003
   #define MINOR_MAI_BINARYIN_TX       0x0004
   #define MINOR_MAI_BINARYIN_RX       0x0005
   #define MINOR_MAI_SETGAIN           0x0006
   #define MINOR_FAI_INTSTART          0x0007
   #define MINOR_FAI_TERMINATE         0x0008
   #define MINOR_FAI_INTSCAN           0x0009
   #define MINOR_FAI_SETPACER          0x000a
   #define MINOR_FAI_ZLP               0x000b
   #define MINOR_FAI_CLEAROVERRUN      0x000c
   #define MINOR_CJCERROR_READ         0x0021
   #define MINOR_CJCERROR_SAVE         0x0022
   #define MINOR_CJCTEMP_READ          0x0023
   #define MINOR_CJCERROR_RESET        0x0024
   #define MINOR_FIXCJCERROR_SAVE      0x0025
   #define MINOR_FIXCJCERROR_LOAD      0x0026
   #define MINOR_START_ADC             0x0030
   #define MINOR_RESET_ADC             0x0031
   #define MINOR_GET_CHLSTATUS_TX      0x0040
   #define MINOR_GET_CHLSTATUS_RX      0x0041
   #define MINOR_SET_CHLSTATUS         0x0042

#define MAJOR_AO                 0x03
   #define MINOR_AO_CONFIG             0x0001
   #define MINOR_AO_BINARYOUT          0x0002

#define MAJOR_COUNTER            0x04
   #define MINOR_CNT_EVENTSTART        0x0001
   #define MINOR_CNT_EVENTREAD_TX      0x0002   // reserved in USB4711
   #define MINOR_CNT_EVENTREAD_RX      0x0003
   #define MINOR_CNT_RESET             0x0004
   #define MINOR_CNT_PULSESTART        0x0005
   #define MINOR_CNT_FREQSTART         0x0006
   #define MINOR_CNT_FREQ_READ_TX      0x0007
   #define MINOR_CNT_FREQ_READ_RX      0x0008
   #define MINOR_CNT_GET_BASECLK       0x0009
   #define MINOR_CNT_PWMSETTING        0x000A
   #define MINOR_CNT_PWMENABLE         0x000B
   #define MINOR_CNT_FREQOUTSTART      0x000C

#define MAJOR_CALIBRATION        0x05
   #define MINOR_CAO_READEEP_TX        0x0001
   #define MINOR_CAO_READEEP_RX        0x0002
   #define MINOR_CAO_WRITEEEP          0x0003
   #define MINOR_CAO_WRITETRIM         0x0004
   #define MINOR_CAI_READEEP_TX        0x0011
   #define MINOR_CAI_READEEP_RX        0x0012
   #define MINOR_CAI_WRITEEEP          0x0013
   #define MINOR_CAI_WRITETRIM         0x0014
   #define MINOR_CAI_RESTORE           0x0021
   #define MINOR_CAI_FULLCALI          0x0022
   #define MINOR_CAI_ZEROCALI          0x0023
   #define MINOR_CAI_FIXFULLCALI       0x0024
   #define MINOR_CAI_FIXZEROCALI       0x0025
   #define MINOR_CAI_COMPLETECALI      0x0026

#define MAJOR_DIRECT_IO          0x10
   #define MINOR_DIRECT_WRITE          0x0001
   #define MINOR_DIRECT_READ           0x0002
   #define MINOR_DIRECT_EE_READ_TX     0x0010
   #define MINOR_DIRECT_EE_READ_RX     0x0011
   #define MINOR_DIRECT_EE_WRITE_TX    0X0012
   #define MINOR_FIXFUNCTIONCALL_TX    0X0013
   #define MINOR_FIXFUNCTIONCALL_RX    0x0014

#define MAJOR_EVENT              0x11
   #define MINOR_EVENT_ENABLE          0x0001

#define MAJOR_SYSTEM		         0x7F
   #define MINOR_READ_SWITCHID         0x0001	
   #define MINOR_WRITE_SWITCHID        0x0002	
   #define MINOR_GET_LAST_ERROR        0x0003	
   #define MINOR_DOWNLOAD_PROG         0x0004
   #define MINOR_GET_FW_VERSION        0x0005
   #define MINOR_USBLED                0x0006
   #define MINOR_DEVICE_OPEN           0x0007
   #define MINOR_DEVICE_CLOSE          0x0008
   #define MINOR_LOCATE                0x0009
   #define MINOR_GET_FW_SIZE           0x000A
   #define MINOR_GET_FW_PAGESIZE_LO    0x000B
   #define MINOR_GET_FW_PAGESIZE_HI    0x000C  
   #define MINOR_GET_USB_HW_INFO       0x000D    
   #define MINOR_GET_USB_FLAG          0x000E
   #define MINOR_DELAY                 0x000F
   #define MINOR_RESETDEVICE           0x0010 

#define MAJOR_TEST               0x80
   #define INT_Disable                 0x0000
   #define INT_SRC_HF                  0x0001
   #define INT_SRC_EMPTY               0x0002
   #define INT_SRC_USER                0x0003

#define MAJOR_TEST_AO            0x90
   #define AO0_Range                   0x00
   #define AO1_Range                   0x01
   #define AO0_OUT                     0x10
   #define AO1_OUT                     0x11
      #define Range0                         0x00
      #define Range1                         0x01
      #define Range2                         0x02
      #define Range3                         0x03
      #define Range4                         0x04
   #define AO0_Cal                     0x20
   #define AO1_Cal                     0x21
      #define Offset_Up                      0x01
      #define Offset_Down                    0x81
      #define Gain_Up                        0x02
      #define Gain_Down                      0x82
   #define AO0_Save_EEP                0x30
   #define AO1_Save_EEP                0x31
   #define AO0_Save_ALL                0x40
   #define AO1_Save_ALL                0x41
   #define AO0_Read_EEP                0x50
   #define AO1_Read_EEP                0x51
      #define Range0                         0x00
      #define Range1                         0x01
      #define Range2                         0x02
      #define Range3                         0x03
      #define Range4                         0x04

#define MAJOR_TEST_AI            0xA0
   #define Set_CH_Range	               0x00  // pBuf: Is HHHHH; HHHHH : Start - Stop - S/D - B/U - Gain
   #define Set_Trigger_Mode            0x01
      #define MASK_SW                        0
      #define MASK_PACER                     1
      #define MASK_EXT                       2
      #define MASK_GATE                      3
   #define Set_Scan_CH                 0x02  // pBuf : Is HHHH : STSP : Start And Stop Channel
   #define Set_INT_SRC                 0x03
   #define Cal_AI                      0x04
      #define Read_AI                        0x00  // pBuf Is Channel Code : 0 ~ F
      #define Offset_Up                      0x01
      #define Offset_Down                    0x81
      #define Gain_Up                        0x02
      #define Gain_Down                      0x82
      #define PGA_Up                         0x03
      #define PGA_Down                       0x83
      #define Save_AI_Gain                   0x04
      #define Save_AI_Offset                 0x05
      #define Save_AI_PGA                    0x06
      #define Read_AI_Par                    0x07
      #define Step1                          0xE1  // pBuf: Is HHHHH; HHHHH : Start - Stop - S/D - B/U - Gain
      #define Step2                          0xE2
      #define Step3                          0xE3  // pBuf: Is GHHH; G : AO0 GainIndex; HHH : AO0 Data
      #define Step4                          0xE4
      #define Step5                          0xE5
      #define Step6                          0xE6
      #define Step7                          0xE7

#define MAJOR_TEST_DIO           0xB0  // pBuf : Is Data
   #define Set_DO_Data                 0x00
   #define Get_DO_Status               0x01
   #define Get_DI_Status               0x02

#define MAJOR_TEST_EEPROM        0xC0
   #define Read_EEPROM                 0x00
   #define Write_EEPROM                0x01
   #define Read_Module_ID              0x02
   #define Write_Module_ID             0x03  // Channel / Offset : Address 0x00 ~ 0xFF; pBuf : Is Data

///////////////////////////////////////////////////////////////////////////////
// Data structures for USB request
///////////////////////////////////////////////////////////////////////////////

#ifdef _USB_MC_PACK_1
#pragma pack(push, 1)
#endif /* _USB_MC_PACK_1 */

typedef struct _BioUsbEnableEvent {
   uint16 EventType;
   uint16 Enabled;
   uint16 EventTrigger; // 0 : Falling; 1 : Rising
} BioUsbEnableEvent, *PBioUsbEnableEvent;

typedef struct _BioUsbDO {
   uint16 Channel;
   uint16 Size;
   uint32 Data;
} BioUsbDO, *PBioUsbDO;

typedef struct _BioUsbDORead_TX {
   uint16 Channel;
   uint16 Size;
} BioUsbDORead_TX, *PBioUsbDORead_TX;

typedef struct _BioUsbDOReadback_RX {
   uint32  Error;
   uint16 Size;
   uint32  Data;
} BioUsbDORead_RX, *PBioUsbDORead_RX;

typedef struct _BioUsbDI_TX {
   uint16 Channel;
   uint16 Size;
} BioUsbDI_TX, *PBioUsbDI_TX;

typedef struct _BioUsbDI_RX {
   uint32  Error;
   uint16  Size;
   uint32  Data;
} BioUsbDI_RX, *PBioUsbDI_RX;

#ifdef PROTOCOL_DIO_EXTEND
typedef struct _BioUsbMDO {
   uint16 StartPort;
   uint16 PortCount;
   uint8  Data[MAX_DO_PORTS];
} BioUsbMDO, *PBioUsbMDO;

typedef struct _BioUsbMDORead_TX {
   uint16 StartPort;
   uint16 PortCount;
} BioUsbMDORead_TX, *PBioUsbMDORead_TX;

typedef struct _BioUsbMDOReadback_RX {
   uint32 Error;
   uint32 Size;
   uint8  Data[MAX_DI_PORTS];
} BioUsbMDORead_RX, *PBioUsbMDORead_RX;

typedef struct _BioUsbMDI_TX {
   uint16 StartPort;
   uint16 PortCount;
} BioUsbMDI_TX, *PBioUsbMDI_TX;

typedef struct _BioUsbMDI_RX {
   uint32 Error;
   uint32 Size;
   uint8  Data[MAX_DI_PORTS];
} BioUsbMDI_RX, *PBioUsbMDI_RX;
#endif

typedef struct _BioUsbAISetGain {
   uint16 Channel;
   uint16 Gain;
#ifdef AI_CHANNEL_CONFIG
   uint16 ChannelConfig;
#endif /* AI_CHANNEL_CONFIG */
} BioUsbAISetGain, *PBioUsbAISetGain;

typedef struct _BioUsbAIBinaryIn_TX {
   uint16 Channel;
   uint16 TriggerMode; // 0 : Software; 1 : External
} BioUsbAIBinaryIn_TX, *PBioUsbAIBinaryIn_TX;

typedef struct _BioUsbAIBinaryIn_RX {
   uint32 Error;
   uint32 Data;
} BioUsbAIBinaryIn_RX, *PBioUsbAIBinaryIn_RX;

typedef struct _BioUsbMaiSetGain {
   uint16 StartChannel;
   uint16 ChannelCount;
#ifdef AI_CHANNEL_CONFIG
   uint16 ChannelConfig;
   uint16 StopChannel;
#endif /* AI_CHANNEL_CONFIG */
#ifdef MAX_AI_CHANNELS
   uint16 Gain[MAX_AI_CHANNELS];
#endif
} BioUsbMaiSetGain, *PBioUsbMaiSetGain;

typedef struct _BioUsbMaiBinaryIn_TX {
   uint16	StartChannel;
   uint16	ChannelCount;
   uint16	TriggerMode;
#ifdef AI_CHANNEL_CONFIG
   uint16  StopChannel;
#endif /* AI_CHANNEL_CONFIG */
} BioUsbMaiBinaryIn_TX, *PBioUsbMaiBinaryIn_TX;

typedef struct _BioUsbMaiBinaryIn_RX {
   uint32 Error;
#ifdef MAX_AI_CHANNELS
   uint32 Data[MAX_AI_CHANNELS];
#endif
} BioUsbMaiBinaryIn_RX, *PBioUsbMaiBinaryIn_RX;

typedef struct _BioUsbFaiIntStart {
   uint16 Channel;
   uint16 Gain;
   uint16 TriggerSource; // 0 : Internal
   // 1 : External
   uint16 PacerDivisor;
   uint32  DataCount;
   uint16 Cyclic;
   uint32  SampleRate;
#ifdef AI_CHANNEL_CONFIG
   uint16 ChannelConfig;
#endif /* AI_CHANNEL_CONFIG */
} BiousbFaiIntStart, *PBioUsbFaiIntStart;

typedef struct _BioUsbFaiIntScanStart {
   uint16 StartChannel;
   uint16 ChannelCount;
   uint16 TriggerSource;
   uint16 PacerDivisor;
   uint32  DataCount;
   uint16 Cyclic;
   uint32  SampleRate;
#ifdef AI_CHANNEL_CONFIG
   uint16 ChannelConfig;
   uint16 StopChannel;
#endif /* AI_CHANNEL_CONFIG */
#ifdef MAX_AI_CHANNELS
   uint16 Gain[MAX_AI_CHANNELS];
#endif
} BioUsbFaiIntScanStart, *PBioUsbFaiIntScanStart;


typedef struct _BioUsbFaiSetPacer {
   uint16 TimerClock;   // Unit : MHz
   uint16 PacerDivisor;
   uint32  SampleRate;   // Unit : Hz
} BioUsbFaiSetPacer, *PBioUsbFaiSetPacer;

typedef struct _BioUsbAOConfig {
   uint16 Channel;
   uint16 ReferenceSource; // 0 : Internal; 1 : External
   uint16 RangeCode;
} BioUsbAOConfig, *PBioUsbAOConfig;

typedef struct _BioUsbAOBinaryOut {
   uint16 Channel;
   uint16 Data;
} BioUsbAOBinaryOut, *PBioUsbAOBinaryOut;

typedef struct _BioUsbCounterEventStart {
   uint16 Counter;
} BioUsbCounterEventStart, *PBioUsbCounterEventStart;

typedef struct _BioUsbCounterEventRead_TX {
   uint16 Counter;
} BioUsbCounterEventRead_TX, *PBioUsbCounterEventRead_TX;

typedef struct _BioUsbCounterEventRead_RX {
   uint32  Error;
   uint32  Count;
   uint16 Overflow;
} BioUsbCounterEventRead_RX, *PBioUsbCounterEventRead_RX;

typedef struct _BioUsbCounterReset {
   uint16 Counter;
} BioUsbCounterReset, *PBioUsbCounterReset;

typedef struct _BioUsbPulseStart {
   uint16 Counter;
#ifdef MAX_COUNTER_PARA
   uint32 Context[MAX_COUNTER_PARA];
#endif /* MAX_COUNTER_PARA */
} BioUsbPulseStart, *PBioUsbPulseStart;

typedef struct _BioUsbPwmSetting {
   uint16 Counter;
#ifdef MAX_COUNTER_PARA
   uint32 Context[MAX_COUNTER_PARA];
#endif /* MAX_COUNTER_PARA */
} BioUsbPwmSetting, *PBioUsbPwmSetting;

typedef struct _BioUsbPwmEnable {
   uint16 Counter;
#ifdef MAX_COUNTER_PARA
   uint32 Context[MAX_COUNTER_PARA];
#endif /* MAX_COUNTER_PARA */
} BioUsbPwmEnable, *PBioUsbPwmEnable;

typedef struct _BioUsbFrequencyStart {
   uint16 Counter;
#ifdef MAX_COUNTER_PARA
   uint32 Context[MAX_COUNTER_PARA];
#endif /* MAX_COUNTER_PARA */
} BioUsbFrequencyStart, *PBioUsbFrequencyStart;

typedef struct _BioUsbFrequencyRead_TX {
   uint16 Counter;
} BioUsbFrequencyRead_TX, *PBioUsbFrequencyRead_TX;

typedef struct _BioUsbFrequencyRead_RX {
   uint32 Error;
#ifdef MAX_COUNTER_PARA
   uint32 Context[MAX_COUNTER_PARA];
#endif /* MAX_COUNTER_PARA */
} BioUsbFrequencyRead_RX, *PBioUsbFrequencyRead_RX;

typedef struct _BioUsbAOCalibrationReadEEP_TX {
   uint16 Channel;
   uint16 RangeCode;
   uint16 OffsetOrGain;         // 0 : Gain; 1 : Offset
   uint16 UserSettingOrDefault; // 1 : User Setting; 2 : Default Setting
} BioUsbAOCalibrationReadEEP_TX, *PBioUsbAOCalibrationReadEEP_TX;

typedef struct _BioUsbAOCalibrationReadEEP_RX {
   uint32 Error;
   uint16 Data;
} BioUsbAOCalibrationReadEEP_RX, *PBioUsbAOCalibrationReadEEP_RX;

typedef struct _BioUsbAOCalibrationWriteEEP {
   uint16 Channel;
   uint16 RangeCode;
   uint16 OffsetOrGain;         // 0 : Gain; 1 : Offset
   uint16 Data;
   uint16 UserSettingOrDefault; // 1 : User Setting; 2 : Default Setting
} BioUsbAOCalibrationWriteEEP, *PBioUsbAOCalibrationWriteEEP;

typedef struct _BioUsbAOCalibrationSetTrim {
   uint16 Channel;
   uint16 OffsetOrGain; // 0 : Gain; 1 : Offset
   uint16 Data;
} BioUsbAOCalibrationSetTrim, *PBioUsbAOCalibrationSetTrim;

typedef struct _BioUsbAICalibrationReadEEP_TX {
   uint16 Type;                 // 0 : ADC_Gain; 1 : ADC_Offset; 2 : PGA_Offset
   uint16 UserSettingOrDefault; // 1 : User Setting; 2 : Default Setting
} BioUsbAICalibrationReadEEP_TX, *PBioUsbAICalibrationReadEEP_TX;

typedef struct _BioUsbAICalibrationReadEEP_RX
{
   uint32  Error;
   uint16 Data;
} BioUsbAICalibrationReadEEP_RX, *PBioUsbAICalibrationReadEEP_RX;

typedef struct _BioUsbAICalibrationSetTrim {
   uint16 Type; // 0 : ADC_Gain; 1 : ADC_Offset; 2 : PGA_Offset;
   uint16 Data;
} BioUsbAICalibrationSetTrim, *PBioUsbAICalibrationSetTrim;

typedef struct _BioUsbGetHardwareInfo {
   uint8 LittleEndian; // 0 : Big Endian; 1 : Little Endian
   uint8 GeneralDL;    // 1 : General Firmware download permitted
} BioUsbGetHardwareInfo, *PBioUsgGetHardwareInfo;

#ifdef _USB_MC_PACK_1
#pragma pack(pop)
#endif /* _USB_MC_PACK_1 */

#endif  // _ADVANTECH_DAQ_USB_PROTOCOL_DEFINE

