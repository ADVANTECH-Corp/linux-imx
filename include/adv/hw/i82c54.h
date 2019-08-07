/*
 * i82c54.h
 *
 *  Created on: 2011-11-25
 *      Author: rocky
 */

#ifndef _I82C54_REGISTER_DEFINE
#define _I82C54_REGISTER_DEFINE

//-------------------------------------------------
// counting type
// 0: Binary counting 16-bits
// 1: Binary coded decimal (BCD) counting
#define I8254_CNT_SHIFT  0
#define I8254_CNT_BITS   1

#define I8254_CNT_BIN    0
#define I8254_CNT_BCD    1

// M2, M1 & M0 Select operating mode
//-------------------------------------------------
// M2 | M1 | M0 | Mode | Description
//-------------------------------------------------
// 0  | 0  | 0  | 0    | Stop on terminal count
// 0  | 0  | 1  | 1    | Programmable one shot
// X  | 1  | 0  | 2    | Rate generator
// X1 | 1  | 1  | 3    | Square wave rate generator
// 1  | 0  | 0  | 4    | Software triggered strobe
// 1  | 0  | 1  | 5    | Hardware triggered strobe
#define I8254_MODE_SHIFT  (I8254_CNT_BITS + I8254_CNT_SHIFT)
#define I8254_MODE_BITS   3

#define I8254_MODE_X(x)   x


// RW1 & RW0 Select read / write operation
//-------------------------------------------------
// Operation                      | RW1 | RW0
//-------------------------------------------------
// Counter latch                  | 0   | 0
// Read/write LSB                 | 0   | 1
// Read/write MSB                 | 1   | 0
// Read/write LSB first, then MSB | 1   | 1
#define I8254_RW_SHIFT     (I8254_MODE_BITS + I8254_MODE_SHIFT)
#define I8254_RW_BITS      2

#define I8254_RW_LATCH     0
#define I8254_RW_LSB       1
#define I8254_RW_MSB       2
#define I8254_RW_LSB_MSB   3

// SC1 & SC0 Select counter
//-------------------------------------------------
// Counter           | SC1 | SC0
//-------------------------------------------------
// 0                 | 0   | 0
// 1                 | 0   | 1
// 2                 | 1   | 0
// Read-back command | 1   | 1
#define I8254_SC_SHIFT  (I8254_RW_BITS + I8254_RW_SHIFT)
#define I8254_SC_BITS   2

#define I8254_SC_CNTR0  0
#define I8254_SC_CNTR1  1
#define I8254_SC_CNTR2  2
#define I8254_SC_RDBK   3

typedef union _I825X_CTL{
   uint8 Value;
   struct {
      uint8 Bcd  : 1;
      uint8 Mode : 3;
      uint8 RW   : 2;
      uint8 SC   : 2;
   };
} I825X_CTL;

//-------------------------------------------------
// Command: Counter latch
//-------------------------------------------------
#define I8254_CMD_LATCH_X(x) (x << I8254_SC_SHIFT)

//-------------------------------------------------
// control word for read back command
//-------------------------------------------------
#define I8254_RDBK_LATCH_NONE 3  //011b
#define I8254_RDBK_LATCH_CNT  2  //010b
#define I8254_RDBK_LATCH_STA  1  //001b
#define I8254_RDBK_LATCH_ALL  0  //000b

typedef union _I825X_CTL_RDBK{
   uint8 Value;
   struct {
      uint8 Dummy  : 1;
      uint8 Cntrs  : 3; // select counter, write a '1' in the corresponding bit to select the
      uint8 Latch  : 2;
      uint8 RDBK   : 2; // MUST be I8254_SC_RDBK
   };
} I825X_CTL_RDBK;

//-------------------------------------------------
// counter status
//-------------------------------------------------
typedef union _I825X_STATUS{
   uint8 Value;
   struct {
      uint8 Bcd  : 1;
      uint8 Mode : 3;
      uint8 RW   : 2;
      uint8 Null : 1;
      uint8 Out  : 1;
   };
}I825X_STATUS;

#endif // _I82C54_REGISTER_DEFINE

