/*
 *
 *  Bluetooth HCI UART driver
 *
 *  Copyright (C) 2000-2001  Qualcomm Incorporated
 *  Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
 *  Copyright (C) 2004-2005  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/poll.h>

#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/signal.h>
#include <linux/ioctl.h>
#include <linux/skbuff.h>

#if defined(BT_AMP) && !defined(MBT_EXT)
#include <amp/bluetooth/bluetooth.h>
#include <amp/bluetooth/hci_core.h>
#else
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#endif

#include "hci_uart.h"
#include "bt_drv.h"

#define VERSION "1.2"

struct h4_struct {
	unsigned long rx_state;
	unsigned long rx_count;
	struct sk_buff *rx_skb;
	struct sk_buff_head txq;
};

/* H4 receiver States */
#define H4_W4_PACKET_TYPE	0
#define H4_W4_EVENT_HDR		1
#define H4_W4_ACL_HDR		2
#define H4_W4_SCO_HDR		3
#define H4_W4_DATA		4

/* Initialize protocol */
static int
h4_open(struct hci_uart *hu)
{
	struct h4_struct *h4;

	PRINTM(CMD, "h4 open %p\n", hu);

	h4 = kzalloc(sizeof(*h4), GFP_ATOMIC);
	if (!h4)
		return -ENOMEM;

	skb_queue_head_init(&h4->txq);

	hu->priv = h4;
	return 0;
}

/* Flush protocol data */
static int
h4_flush(struct hci_uart *hu)
{
	struct h4_struct *h4 = hu->priv;

	PRINTM(CMD, "h4 flush hu %p\n", hu);

	skb_queue_purge(&h4->txq);

	return 0;
}

/* Close protocol */
static int
h4_close(struct hci_uart *hu)
{
	struct h4_struct *h4 = hu->priv;

	hu->priv = NULL;

	PRINTM(CMD, "h4 close hu %p\n", hu);

	skb_queue_purge(&h4->txq);

	if (h4->rx_skb)
		kfree_skb(h4->rx_skb);

	hu->priv = NULL;
	kfree(h4);

	return 0;
}

/* Enqueue frame for transmittion (padding, crc, etc) */
static int
h4_enqueue(struct hci_uart *hu, struct sk_buff *skb)
{
	struct h4_struct *h4 = hu->priv;

	PRINTM(INFO, "h4 enqueue skb %p\n", skb);

	/* Prepend skb with frame type */
	memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);
	skb_queue_tail(&h4->txq, skb);

	return 0;
}

static inline int
h4_check_data_len(struct h4_struct *h4, int len)
{
	register int room = skb_tailroom(h4->rx_skb);

	PRINTM(INFO, "h4_check_data_len: len %d room %d\n", len, room);

	if (!len) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#if defined(BT_AMP) && !defined(MBT_EXT)
		hci_recv_frame(h4->rx_skb);
#else
		hci_recv_frame((struct hci_dev *)h4->rx_skb->dev, h4->rx_skb);
#endif
#else
		hci_recv_frame(h4->rx_skb);
#endif
	} else if (len > room) {
		PRINTM(ERROR, "Data length is too large\n");
		kfree_skb(h4->rx_skb);
	} else {
		h4->rx_state = H4_W4_DATA;
		h4->rx_count = len;
		return len;
	}

	h4->rx_state = H4_W4_PACKET_TYPE;
	h4->rx_skb = NULL;
	h4->rx_count = 0;

	return 0;
}

/* Recv data */
static int
h4_recv(struct hci_uart *hu, void *data, int count)
{
	struct h4_struct *h4 = hu->priv;
	register char *ptr;
	struct hci_event_hdr *eh;
	struct hci_acl_hdr *ah;
	struct hci_sco_hdr *sh;
	register int len, type = 0, dlen;

	struct hci_dev *hdev = NULL;
	struct m_dev *mdev_fm = &(hu->m_dev[FM_SEQ]);
	struct m_dev *mdev_nfc = &(hu->m_dev[NFC_SEQ]);
	struct nfc_dev *nfc_dev =
		(struct nfc_dev *)hu->m_dev[NFC_SEQ].dev_pointer;
	struct fm_dev *fm_dev = (struct fm_dev *)hu->m_dev[FM_SEQ].dev_pointer;

	hdev = (struct hci_dev *)hu->hdev;

	PRINTM(INFO, "h4_recv: hu %p count %d rx_state %ld rx_count %ld\n",
	       hu, count, h4->rx_state, h4->rx_count);

	ptr = data;
	while (count) {
		if (h4->rx_count) {
			len = min_t(unsigned int, h4->rx_count, count);
			memcpy(skb_put(h4->rx_skb, len), ptr, len);
			h4->rx_count -= len;
			count -= len;
			ptr += len;

			if (h4->rx_count)
				continue;

			switch (h4->rx_state) {
			case H4_W4_DATA:
				type = bt_cb(h4->rx_skb)->pkt_type;
				PRINTM(DATA, "H4 Rx: type=%d len=%d\n", type,
				       h4->rx_skb->len);
#ifdef CONFIG_BT_HCIUART_PS
				if (type == HCI_EVENT_PKT)
#ifdef CONFIG_MULTI_CARD_PS
					ps_check_event_packet(hu, h4->rx_skb);
#else
					ps_check_event_packet(h4->rx_skb);
#endif
#endif
				if (type == HCI_EVENT_PKT) {
					switch (h4->rx_skb->data[0]) {
					case 0x0E:
							/** cmd complete */
						if (h4->rx_skb->data[3] == 0x80
						    && h4->rx_skb->data[4] ==
						    0xFE) {
								/** FM cmd complete */
							if (fm_dev) {
								h4->rx_skb->
									dev =
									(void *)
									mdev_fm;
								mdev_recv_frame
									(h4->
									 rx_skb);
								mdev_fm->stat.
									byte_rx
									+=
									(len +
									 HCI_EVENT_HDR_SIZE);
							}
						} else if (h4->rx_skb->
							   data[3] == 0x81 &&
							   h4->rx_skb->
							   data[4] == 0xFE) {
								/** NFC cmd complete */
							if (nfc_dev) {
								h4->rx_skb->
									dev =
									(void *)
									mdev_nfc;
								mdev_recv_frame
									(h4->
									 rx_skb);
								mdev_nfc->stat.
									byte_rx
									+=
									(len +
									 HCI_EVENT_HDR_SIZE);
							}
						} else {
							if (hdev) {
								h4->rx_skb->
									dev =
									(void *)
									hdev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#if defined(BT_AMP) && !defined(MBT_EXT)
								hci_recv_frame
									(h4->
									 rx_skb);
#else
								hci_recv_frame
									(hdev,
									 h4->
									 rx_skb);
#endif
#else
								hci_recv_frame
									(h4->
									 rx_skb);
#endif
								hdev->stat.
									byte_rx
									+=
									(len +
									 HCI_EVENT_HDR_SIZE);
							}
						}
						break;
					case 0x0F:
							/** cmd status */
						if (h4->rx_skb->data[4] == 0x80
						    && h4->rx_skb->data[5] ==
						    0xFE) {
								/** FM cmd ststus */
							if (fm_dev) {
								h4->rx_skb->
									dev =
									(void *)
									mdev_fm;
								mdev_recv_frame
									(h4->
									 rx_skb);
								mdev_fm->stat.
									byte_rx
									+=
									(len +
									 HCI_EVENT_HDR_SIZE);
							}
						} else if (h4->rx_skb->
							   data[4] == 0x81 &&
							   h4->rx_skb->
							   data[5] == 0xFE) {
								/** NFC cmd ststus */
							if (nfc_dev) {
								h4->rx_skb->
									dev =
									(void *)
									mdev_nfc;
								mdev_recv_frame
									(h4->
									 rx_skb);
								mdev_nfc->stat.
									byte_rx
									+=
									(len +
									 HCI_EVENT_HDR_SIZE);
							}
						} else {
								/** BT cmd status */
							if (hdev) {
								h4->rx_skb->
									dev =
									(void *)
									hdev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#if defined(BT_AMP) && !defined(MBT_EXT)
								hci_recv_frame
									(h4->
									 rx_skb);
#else
								hci_recv_frame
									(hdev,
									 h4->
									 rx_skb);
#endif
#else
								hci_recv_frame
									(h4->
									 rx_skb);
#endif
								hdev->stat.
									byte_rx
									+=
									(len +
									 HCI_EVENT_HDR_SIZE);
							}
						}
						break;
					case 0xFF:
							/** Vendor specific pkt */
						if (h4->rx_skb->data[2] == 0xC0) {
								/** NFC EVT */
							if (nfc_dev) {
								h4->rx_skb->
									dev =
									(void *)
									mdev_nfc;
								mdev_recv_frame
									(h4->
									 rx_skb);
								mdev_nfc->stat.
									byte_rx
									+=
									(len +
									 HCI_EVENT_HDR_SIZE);
							}
						} else if (h4->rx_skb->
							   data[2] >= 0x80 &&
							   h4->rx_skb->
							   data[2] <= 0xAF) {
								/** FM EVT */
							if (fm_dev) {
								h4->rx_skb->
									dev =
									(void *)
									mdev_fm;
								mdev_recv_frame
									(h4->
									 rx_skb);
								mdev_fm->stat.
									byte_rx
									+=
									(len +
									 HCI_EVENT_HDR_SIZE);
							}
						} else {
								/** BT EVT */
							if (hdev) {
								h4->rx_skb->
									dev =
									(void *)
									hdev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#if defined(BT_AMP) && !defined(MBT_EXT)
								hci_recv_frame
									(h4->
									 rx_skb);
#else
								hci_recv_frame
									(hdev,
									 h4->
									 rx_skb);
#endif
#else
								hci_recv_frame
									(h4->
									 rx_skb);
#endif
								hdev->stat.
									byte_rx
									+=
									(len +
									 HCI_EVENT_HDR_SIZE);
							}
						}
						break;
					default:
							/** BT EVT */

						if (hdev) {
							h4->rx_skb->dev =
								(void *)hdev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#if defined(BT_AMP) && !defined(MBT_EXT)
							hci_recv_frame(h4->
								       rx_skb);
#else
							hci_recv_frame(hdev,
								       h4->
								       rx_skb);
#endif
#else
							hci_recv_frame(h4->
								       rx_skb);
#endif
							hdev->stat.byte_rx +=
								(len +
								 HCI_EVENT_HDR_SIZE);
						}

						break;
					}
				} else {
					if (hdev) {
						h4->rx_skb->dev = (void *)hdev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#if defined(BT_AMP) && !defined(MBT_EXT)
						hci_recv_frame(h4->rx_skb);
#else
						hci_recv_frame(hdev,
							       h4->rx_skb);
#endif
#else
						hci_recv_frame(h4->rx_skb);
#endif
					}
					if (type == HCI_ACLDATA_PKT)
						hdev->stat.byte_rx +=
							(len +
							 HCI_ACL_HDR_SIZE);
					else if (type == HCI_SCODATA_PKT)
						hdev->stat.byte_rx +=
							(len +
							 HCI_SCO_HDR_SIZE);
				}

				h4->rx_state = H4_W4_PACKET_TYPE;
				h4->rx_skb = NULL;
				continue;

			case H4_W4_EVENT_HDR:
				eh = hci_event_hdr(h4->rx_skb);
				PRINTM(INFO,
				       "Event header: evt 0x%2.2x plen %d\n",
				       eh->evt, eh->plen);

				h4_check_data_len(h4, eh->plen);
				continue;

			case H4_W4_ACL_HDR:
				ah = hci_acl_hdr(h4->rx_skb);
				dlen = __le16_to_cpu(ah->dlen);

				PRINTM(INFO, "ACL header: dlen %d\n", dlen);

				h4_check_data_len(h4, dlen);
				continue;

			case H4_W4_SCO_HDR:
				sh = hci_sco_hdr(h4->rx_skb);

				PRINTM(INFO, "SCO header: dlen %d\n", sh->dlen);

				h4_check_data_len(h4, sh->dlen);
				continue;
			}
		}

		/* H4_W4_PACKET_TYPE */
		switch (*ptr) {
		case HCI_EVENT_PKT:
			PRINTM(INFO, "Event packet\n");
			h4->rx_state = H4_W4_EVENT_HDR;
			h4->rx_count = HCI_EVENT_HDR_SIZE;
			type = HCI_EVENT_PKT;
			break;

		case HCI_ACLDATA_PKT:
			PRINTM(INFO, "ACL packet\n");
			h4->rx_state = H4_W4_ACL_HDR;
			h4->rx_count = HCI_ACL_HDR_SIZE;
			type = HCI_ACLDATA_PKT;
			break;

		case HCI_SCODATA_PKT:
			PRINTM(INFO, "SCO packet\n");
			h4->rx_state = H4_W4_SCO_HDR;
			h4->rx_count = HCI_SCO_HDR_SIZE;
			type = HCI_SCODATA_PKT;
			break;

		default:
			PRINTM(ERROR, "Unknown HCI packet type %2.2x\n",
			       (__u8) * ptr);
			hu->hdev->stat.err_rx++;
			ptr++;
			count--;
			continue;
		};

		ptr++;
		count--;

		/* Allocate packet */
		h4->rx_skb = bt_skb_alloc(HCI_MAX_FRAME_SIZE, GFP_ATOMIC);
		if (!h4->rx_skb) {
			PRINTM(ERROR, "Can't allocate mem for new packet\n");
			h4->rx_state = H4_W4_PACKET_TYPE;
			h4->rx_count = 0;
			return 0;
		}

		bt_cb(h4->rx_skb)->pkt_type = type;
	}

	return count;
}

static struct sk_buff *
h4_dequeue(struct hci_uart *hu)
{
	struct h4_struct *h4 = hu->priv;
	return skb_dequeue(&h4->txq);
}

static struct hci_uart_proto h4p = {
	.id = HCI_UART_H4,
	.open = h4_open,
	.close = h4_close,
	.recv = h4_recv,
	.enqueue = h4_enqueue,
	.dequeue = h4_dequeue,
	.flush = h4_flush,
};

int
h4_init(void)
{
	int err = hci_uart_register_proto(&h4p);

	if (!err)
		PRINTM(MSG, "HCI H4 protocol initialized\n");
	else
		PRINTM(ERROR, "HCI H4 protocol registration failed");

	return err;
}

int
h4_deinit(void)
{
	return hci_uart_unregister_proto(&h4p);
}
