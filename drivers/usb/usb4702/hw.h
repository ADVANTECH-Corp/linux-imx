/*
 * hw.h
 *
 *  Created on: August 1, 2012
 *      Author: rocky
 */

#ifndef _KERNEL_MODULE_HW_H_
#define _KERNEL_MODULE_HW_H_

///--------------------------------------------------------------
///          USB Status Define
///--------------------------------------------------------------
#define USB_FLAG_AIBUZY        0x1<<0
#define USB_FLAG_FAIOVERRUN    0x1<<8
#define USB_FLAG_FAITERMINATE  0x1<<9
#define USB_FLAG_EPBUFFERFULL  0x1<<11

#endif /* _KERNEL_MODULE_HW_H_ */
