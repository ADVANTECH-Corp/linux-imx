/*******************************************************************************
                  Copyright (c) 1983-2009 Advantech Co., Ltd.
********************************************************************************
THIS IS AN UNPUBLISHED WORK CONTAINING CONFIDENTIAL AND PROPRIETARY INFORMATION
WHICH IS THE PROPERTY OF ADVANTECH CORP., ANY DISCLOSURE, USE, OR REPRODUCTION,
WITHOUT WRITTEN AUTHORIZATION FROM ADVANTECH CORP., IS STRICTLY PROHIBITED. 

================================================================================
REVISION HISTORY
--------------------------------------------------------------------------------
$Log: /Project/BionicDAQ/Windows/Public/Inc/BDaqDef.h $
 
 Revision: 1   Date: 2009-09-25 06:50:28Z   User: kang.ning 
 Baseline 
--------------------------------------------------------------------------------
$NoKeywords:  $
*/

/****************************************************************************
*                                                                           *
* BDaqDef.h -- Bionic DAQ Type Definitions                                  *
*                                                                           *
****************************************************************************/
#ifndef _BIONIC_DAQ_TYPE_DEF_H
#define _BIONIC_DAQ_TYPE_DEF_H

#ifndef _BDAQ_TYPES_ONLY
#  define _BDAQ_TYPES_ONLY
#endif
#ifndef _BDAQ_NO_NAMESPACE
#  define _BDAQ_NO_NAMESPACE
#endif
#include "bdaqctrl.h"

#if !defined(__min) && !defined(__max)
#  define __max(x, y) (((x) > (y)) ? (x) : (y))
#  define __min(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef IN
#  define IN        // Place holder indicates an input parameter
#endif
#ifndef OUT
#  define OUT       // Place holder indicates a output parameter
#endif
#ifndef OPTIONAL    // Place holder indicates an optional parameter,
#  define OPTIONAL  // which value can be zero or NULL, depends on the parameter type.
#endif

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))
#endif

#ifndef offset_of
#  ifdef __compiler_offsetof
#     define offset_of(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#  else
#     define offset_of(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#  endif
#endif

#ifndef container_of
#  ifdef __linux__
#     define container_of(ptr, type, member) ({         \
         const typeof( ((type *)0)->member ) *__mptr = (ptr);  \
         (type *)( (char *)__mptr - offset_of(type,member) );})
#  else
#     define container_of(ptr, type, member)\
         ((type *)((char*)(ptr) - offset_of(type,member)))
#  endif
#endif

#define DBGIO_STACKBUF_THR       32
#define DBGIO_MAX_LENGTH         4096

#define DAQ_ACQ_FINITE_SYNC      0x0
#define DAQ_ACQ_FINITE_ASYNC     0x1
#define DAQ_ACQ_INFINITE         0x2
#define DAQ_XFER_INT             0x0
#define DAQ_XFER_DMA             0x100

#define DAQ_FN_IDLE              0
#define DAQ_FN_READY             1
#define DAQ_FN_RUNNING           2
#define DAQ_FN_STOPPED           3
#define DAQ_FN_PASSED            4

#define DAQ_IN_DATAREADY                0x1
#define DAQ_IN_BUF_OVERRUN              0x2
#define DAQ_IN_BUF_FULL                 0x4
#define DAQ_IN_CACHE_OVERFLOW           0x8
#define DAQ_IN_BURN_OUT                 0x10
#define DAQ_IN_TSTAMP_OVERRUN           0x20
#define DAQ_IN_TSTAMP_CACHE_OVERFLOW    0x40
#define DAQ_IN_MARK_OVERRUN             0x80

#define DAQ_IN_MUST_STOP(state, mode)   ((state & DAQ_IN_BUF_FULL) && !(mode == DAQ_ACQ_INFINITE))

#define DAQ_OUT_TRANSMITTED      0x1
#define DAQ_OUT_BUF_UNDERRUN     0x2
#define DAQ_OUT_TRANSSTOPPED     0x4
#define DAQ_OUT_CACHE_EMPTY      0x8


//
// Header part of the structure: DEVICE_SHARED
// Usage:
// typedef struct xxxxx {
//    kshr_header Header;
//    ......
// } xxx;
//
typedef struct kshr_header {
   uint32 size;         // size of the whole 'DEVICE_SHARED' structure, this is used to identify the different version of the structure.
   uint32 prod_id;      // Device Type
   uint32 dev_number;   // Zero-based device number
} kshr_header;


#endif /* _BIONIC_DAQ_TYPE_DEF_H */
