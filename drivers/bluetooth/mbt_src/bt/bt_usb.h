/** @file bt_usb.h
 *  @brief This file contains USB (interface) related macros, enum, and structure.
 *
 * Copyright (C) 2007-2018, Marvell International Ltd.
 *
 * This software file (the "File") is distributed by Marvell International
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991
 * (the "License").  You may use, redistribute and/or modify this File in
 * accordance with the terms and conditions of the License, a copy of which
 * is available along with the File in the gpl.txt file or by writing to
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 * this warranty disclaimer.
 *
 */

#ifndef _BT_USB_H_
#define _BT_USB_H_

#include <linux/usb.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 24)
#define REFDATA __refdata
#else
#define REFDATA
#endif

/** USB 8897 VID 1 */
#define USB8897_VID_1   0x1286
/** USB 8897 PID 2 */
#define USB8897_PID_2   0x2046
/** USB8897 card type */
#define CARD_TYPE_USB8897   0x01

/** USB8766 card type */
#define CARD_TYPE_USB8766   0x03

/** Boot state: FW download */
#define USB_FW_DNLD 1
/** Boot state: FW ready */
#define USB_FW_READY 2

/* Transmit buffer size for chip revision check */
#define CHIP_REV_TX_BUF_SIZE    16
/* Receive buffer size for chip revision check */
#define CHIP_REV_RX_BUF_SIZE    2048

/* Extensions */
#define EXTEND_HDR       (0xAB95)
#define EXTEND_V1        (0x0001)

/** bt_usb_ep */
typedef enum _bt_usb_ep {
	BT_USB_EP_CMD = 0,	/* Control Endpoint */
	BT_USB_EP_CMD_EVENT = 1,
	BT_USB_EP_EVENT = 3,	/* Interrupt Endpoint */
	BT_USB_EP_ACL_DATA = 4,	/* Bulk Endpoint */
	BT_USB_EP_SCO_DATA = 5,	/* Isochronous Endpoint */
} bt_usb_ep;

/** Timeout in milliseconds for usb_bulk_msg function */
#define BT_USB_BULK_MSG_TIMEOUT      100

/** Timeout in milliseconds for usb_control_msg function */
#define BT_USB_CTRL_MSG_TIMEOUT      100

/** Data Structures */
/** driver_buffer data structure */
typedef struct _bt_usb_buffer {
	/** Flags for this buffer */
	u32 flags;
	/** Buffer descriptor, e.g. skb in Linux */
	void *pdesc;
	/** Pointer to buffer */
	u8 *pbuf;
	/** Offset to data */
	u32 data_offset;
	/** Data length */
	u32 data_len;
} bt_usb_buffer, *pbt_usb_buffer;

/** Card-type detection frame response */
struct usb_chip_rev_resp {
	/** 32-bit ACK+WINNER field */
	u32 ack_winner;
	/** 32-bit Sequence number */
	u32 seq;
	/** 32-bit extend */
	u32 extend;
	/** 32-bit chip-revision code */
	u32 chip_rev;
};

/** USB card description structure*/
struct usb_card_rec {
	/** USB device */
	struct usb_device *udev;

	/** USB curr interface */
	struct usb_interface *intf;
	/** Rx data endpoint address */
	u8 rx_cmd_ep;

    /** Tx command endpoint address */
	u8 tx_cmd_ep;

    /** Bulk out max packet size */
	int bulk_out_maxpktsize;

	/** USB isoc interface */
	struct usb_interface *isoc;

	/** USB Tx URB anchor */
	struct usb_anchor tx_anchor;

    /** USB Rx Intr URB anchor */
	struct usb_anchor intr_anchor;

    /** USB Rx Bulk URB anchor */
	struct usb_anchor bulk_anchor;

    /** USB Rx Isoc URB anchor */
	struct usb_anchor isoc_anchor;

    /** Tx counter */
	int tx_in_flight;

    /** Tx lock */
	spinlock_t txlock;

    /** Rx lock */
	spinlock_t rxlock;

	/** current flags */
	unsigned long flags;

	/** Isoc work */
	struct work_struct work;

	/** driver specific struct */
	void *priv;

	/** Flag to indicate boot state */
	u8 boot_state;

	/** Command request type */
	u8 cmdreq_type;

	/** Rx Interrupt endpoint address */
	struct usb_endpoint_descriptor *intr_ep;

	/** Tx Bulk endpoint address */
	struct usb_endpoint_descriptor *bulk_tx_ep;

	/** Rx Bulk endpoint address */
	struct usb_endpoint_descriptor *bulk_rx_ep;

	/** Tx Isoc endpoint address */
	struct usb_endpoint_descriptor *isoc_tx_ep;

	/** Rx Isoc endpoint address */
	struct usb_endpoint_descriptor *isoc_rx_ep;

	/** SCO No. */
	unsigned int sco_num;

	/** Isoc Alt Setting*/
	int isoc_altsetting;

    /** USB card type */
	int card_type;
};

#endif /* _BT_USB_H_ */
