#ifdef GPL_FILE
/** @file bt_drv.h
  * @brief This header file contains global constant/enum definitions,
  * global variable declaration.
  *
  * Copyright 2014-2020 NXP
  *
  * This software file (the File) is distributed by NXP
  * under the terms of the GNU General Public License Version 2, June 1991
  * (the License).  You may use, redistribute and/or modify the File in
  * accordance with the terms and conditions of the License, a copy of which
  * is available by writing to the Free Software Foundation, Inc.,
  * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
  * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
  * this warranty disclaimer.
  *
  */
#else
/** @file bt_drv.h
  * @brief This header file contains global constant/enum definitions,
  * global variable declaration.
  *
  * Copyright 2014-2020 NXP
  *
  * NXP CONFIDENTIAL
  * The source code contained or described herein and all documents related to
  * the source code (Materials) are owned by NXP, its
  * suppliers and/or its licensors. Title to the Materials remains with NXP,
  * its suppliers and/or its licensors. The Materials contain
  * trade secrets and proprietary and confidential information of NXP, its
  * suppliers and/or its licensors. The Materials are protected by worldwide copyright
  * and trade secret laws and treaty provisions. No part of the Materials may be
  * used, copied, reproduced, modified, published, uploaded, posted,
  * transmitted, distributed, or disclosed in any way without NXP's prior
  * express written permission.
  *
  * No license under any patent, copyright, trade secret or other intellectual
  * property right is granted to or conferred upon you by disclosure or delivery
  * of the Materials, either expressly, by implication, inducement, estoppel or
  * otherwise. Any license under such intellectual property rights must be
  * express and approved by NXP in writing.
  *
  */
#endif

#ifndef _BT_DRV_H_
#define _BT_DRV_H_

#ifndef BIT
/** BIT definition */
#define BIT(x) (1UL << (x))
#endif

#ifdef MBT_64BIT
typedef u64 t_ptr;
#else
typedef u32 t_ptr;
#endif

#define MBTCHAR_MAJOR_NUM    (0)

/** Define drv_mode bit */
#define DRV_MODE_BT         BIT(0)
#define DRV_MODE_FM        BIT(1)
#define DRV_MODE_NFC       BIT(2)

/** Define devFeature bit */
#define DEV_FEATURE_BT     BIT(0)
#define DEV_FEATURE_BTAMP     BIT(1)
#define DEV_FEATURE_BLE     BIT(2)
#define DEV_FEATURE_FM     BIT(3)
#define DEV_FEATURE_NFC     BIT(4)

/** Define maximum number of radio func supported */
#define MAX_RADIO_FUNC     3

/** Debug Macro Definition */

extern u32 drvdbg;

/** Debug level : Message */
#define	DBG_MSG			BIT(0)
/** Debug level : Fatal */
#define DBG_FATAL		BIT(1)
/** Debug level : Error */
#define DBG_ERROR		BIT(2)
/** Debug level : Data */
#define DBG_DATA		BIT(3)
/** Debug level : Command */
#define DBG_CMD			BIT(4)
/** Debug level : Event */
#define DBG_EVENT		BIT(5)
/** Debug level : Interrupt */
#define DBG_INTR		BIT(6)

/** Debug entry : Data dump */
#define DBG_DAT_D		BIT(16)
/** Debug entry : Data dump */
#define DBG_CMD_D		BIT(17)

/** Debug level : Entry */
#define DBG_ENTRY		BIT(28)
/** Debug level : Warning */
#define DBG_WARN		BIT(29)
/** Debug level : Informative */
#define DBG_INFO		BIT(30)

/** Print informative message */
#define	PRINTM_INFO(msg...)  do {} while(0)
/** Print warning message */
#define	PRINTM_WARN(msg...)  do {} while(0)
/** Print entry message */
#define	PRINTM_ENTRY(msg...) do {} while(0)

/** Print interrupt message */
#define	PRINTM_INTR(msg...)  do {if (drvdbg & DBG_INTR)  printk(KERN_DEBUG msg);} while(0)
/** Print event message */
#define	PRINTM_EVENT(msg...) do {if (drvdbg & DBG_EVENT) printk(KERN_DEBUG msg);} while(0)
/** Print command message */
#define	PRINTM_CMD(msg...)   do {if (drvdbg & DBG_CMD)   printk(KERN_DEBUG msg);} while(0)
/** Print data message */
#define	PRINTM_DATA(msg...)  do {if (drvdbg & DBG_DATA)  printk(KERN_DEBUG msg);} while(0)
/** Print error message */
#define	PRINTM_ERROR(msg...) do {if (drvdbg & DBG_ERROR) printk(KERN_ERR msg);} while(0)
/** Print fatal message */
#define	PRINTM_FATAL(msg...) do {if (drvdbg & DBG_FATAL) printk(KERN_ERR msg);} while(0)
/** Print message */
#define	PRINTM_MSG(msg...)   do {if (drvdbg & DBG_MSG)   printk(KERN_ALERT msg);} while(0)

/** Print data dump message */
#define	PRINTM_DAT_D(msg...)  do {if (drvdbg & DBG_DAT_D)  printk(KERN_DEBUG msg);} while(0)
/** Print data dump message */
#define	PRINTM_CMD_D(msg...)  do {if (drvdbg & DBG_CMD_D)  printk(KERN_DEBUG msg);} while(0)

/** Print message with required level */
#define	PRINTM(level,msg...) PRINTM_##level(msg)

/** Debug dump buffer length */
#define DBG_DUMP_BUF_LEN 	64
/** Maximum number of dump per line */
#define MAX_DUMP_PER_LINE	16
/** Maximum data dump length */
#define MAX_DATA_DUMP_LEN	48

static inline void
hexdump(char *prompt, u8 * buf, int len)
{
	int i;
	char dbgdumpbuf[DBG_DUMP_BUF_LEN];
	char *ptr = dbgdumpbuf;

	printk(KERN_DEBUG "%s: len=%d\n", prompt, len);
	for (i = 1; i <= len; i++) {
		ptr += snprintf(ptr, 4, "%02x ", *buf);
		buf++;
		if (i % MAX_DUMP_PER_LINE == 0) {
			*ptr = 0;
			printk(KERN_DEBUG "%s\n", dbgdumpbuf);
			ptr = dbgdumpbuf;
		}
	}
	if (len % MAX_DUMP_PER_LINE) {
		*ptr = 0;
		printk(KERN_DEBUG "%s\n", dbgdumpbuf);
	}
}

/** Debug hexdump of debug data */
#define DBG_HEXDUMP_ERROR(x,y,z)     do {if (drvdbg & DBG_ERROR) hexdump(x,y,z);} while(0)
#define DBG_HEXDUMP_DAT_D(x,y,z)     do {if (drvdbg & DBG_DAT_D) hexdump(x,y,z);} while(0)
/** Debug hexdump of debug command */
#define DBG_HEXDUMP_CMD_D(x,y,z)     do {if (drvdbg & DBG_CMD_D) hexdump(x,y,z);} while(0)

/** Debug hexdump */
#define DBG_HEXDUMP(level,x,y,z)    DBG_HEXDUMP_##level(x,y,z)

/** Mark entry point */
#define ENTER()                 PRINTM(ENTRY, "Enter: %s, %s:%i\n", __FUNCTION__, \
                                                        __FILE__, __LINE__)
/** Mark exit point */
#define LEAVE()                 PRINTM(ENTRY, "Leave: %s, %s:%i\n", __FUNCTION__, \
                                                        __FILE__, __LINE__)

/** Length of device name */
#define DEV_NAME_LEN				32
/** Bluetooth upload size */
#define	BT_UPLD_SIZE				2312
/** Bluetooth status success */
#define BT_STATUS_SUCCESS			(0)
/** Bluetooth status failure */
#define BT_STATUS_FAILURE			(-1)

#ifndef	TRUE
/** True value */
#define TRUE			1
#endif
#ifndef	FALSE
/** False value */
#define	FALSE			0
#endif

/** Set thread state */
#define OS_SET_THREAD_STATE(x)		set_current_state(x)
/** Time to wait until Host Sleep state change in millisecond */
#define WAIT_UNTIL_HS_STATE_CHANGED 2000
/** Time to wait cmd resp in millisecond */
#define WAIT_UNTIL_CMD_RESP	    5000

/** Sleep until a condition gets true or a timeout elapses */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#define os_wait_interruptible_timeout(waitq, cond, timeout) \
	interruptible_sleep_on_timeout(&waitq, ((timeout) * HZ / 1000))
#else
#define os_wait_interruptible_timeout(waitq, cond, timeout) \
	wait_event_interruptible_timeout(waitq, cond, ((timeout) * HZ / 1000))
#endif

#ifndef __ATTRIB_ALIGN__
#define __ATTRIB_ALIGN__ __attribute__((aligned(4)))
#endif

#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__ __attribute__ ((packed))
#endif

/** Disable interrupt */
#define OS_INT_DISABLE	spin_lock_irqsave(&priv->driver_lock, priv->driver_flags)
/** Enable interrupt */
#define	OS_INT_RESTORE	spin_unlock_irqrestore(&priv->driver_lock, priv->driver_flags)

#ifndef HCI_BT_AMP
/** BT_AMP flag for device type */
#define  HCI_BT_AMP			0x80
#endif

/** Device type of BT */
#define DEV_TYPE_BT             0x00
/** Device type of AMP */
#define DEV_TYPE_AMP			0x01
/** Device type of FM */
#define DEV_TYPE_FM                    0x02
/** Device type of NFC */
#define DEV_TYPE_NFC                  0x04

#ifndef MAX
/** Return maximum of two */
#define MAX(a,b)		((a) > (b) ? (a) : (b))
#endif

/** This is for firmware specific length */
#define HCI_EXTRA_LEN			36

/** Command buffer size for Nxp driver */
#define MRVDRV_SIZE_OF_CMD_BUFFER       (2 * 1024)

/** Bluetooth Rx packet buffer size for Nxp driver */
#define MRVDRV_BT_RX_PACKET_BUFFER_SIZE \
	(HCI_MAX_FRAME_SIZE + HCI_EXTRA_LEN)

#ifdef SD
/** Buffer size to allocate */
#define ALLOC_BUF_SIZE		(((MAX(MRVDRV_BT_RX_PACKET_BUFFER_SIZE, \
					MRVDRV_SIZE_OF_CMD_BUFFER) + SDIO_HEADER_LEN \
					+ SD_BLOCK_SIZE - 1) / SD_BLOCK_SIZE) * SD_BLOCK_SIZE)
#else
#define ALLOC_BUF_SIZE		(MAX(MRVDRV_BT_RX_PACKET_BUFFER_SIZE, \
					MRVDRV_SIZE_OF_CMD_BUFFER) + BT_HEADER_LEN )
#endif

/** Request FW timeout in second */
#define REQUEST_FW_TIMEOUT		30

/** The number of times to try when polling for status bits */
#define MAX_POLL_TRIES			100

#ifdef MULTI_INTERFACE_DNLD
/** The number of times to try when waiting for downloaded firmware to
    become active when multiple interface is present */
#define MAX_MULTI_INTERFACE_POLL_TRIES  1000
#endif

/** The number of times to try when waiting for downloaded firmware to
     become active. (polling the scratch register). */
#define MAX_FIRMWARE_POLL_TRIES		100

#ifdef BT_SLEEP_TIMEOUT
/** default idle time */
#define DEFAULT_IDLE_TIME           0
#endif /* BT_SLEEP_TIMEOUT */

#endif /* _BT_DRV_H_ */
