/** @file bt_usb.c
 *  @brief This file contains USB (interface) related functions.
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

#include "bt_drv.h"
#include "bt_usb.h"

extern bt_private *m_priv[];

/********************************************************
		Local Variables
********************************************************/

/** Marvell USB device and interface */
#define MARVELL_USB_DEVICE_AND_IFACE(vid, pid, cl, sc, pr, name) \
	USB_DEVICE_AND_INTERFACE_INFO(vid, pid, cl, sc, pr),\
	.driver_info = (t_ptr)name

/** Name of the USB driver */
static const char usbdriver_name[] = "bt-usb8xxx";

/** This structure contains the device signature */
struct usb_device_id bt_usb_table[] = {

	/* Enter the device signature inside */
	{MARVELL_USB_DEVICE_AND_IFACE(USB8897_VID_1, USB8897_PID_2,
				      USB_CLASS_WIRELESS_CONTROLLER, 1, 1,
				      "Marvell BT USB Adapter")},
	/* Terminating entry */
	{},
};

static int bt_usb_probe(struct usb_interface *intf,
			const struct usb_device_id *id);
static void bt_usb_disconnect(struct usb_interface *intf);
#if defined(CONFIG_PM)
static int bt_usb_suspend(struct usb_interface *intf, pm_message_t message);
static int bt_usb_resume(struct usb_interface *intf);
#endif

/** bt_usb_driver */
static struct usb_driver REFDATA bt_usb_driver = {
	/* Driver name */
	.name = usbdriver_name,

	/* Probe function name */
	.probe = bt_usb_probe,

	/* Disconnect function name */
	.disconnect = bt_usb_disconnect,

	/* Device signature table */
	.id_table = bt_usb_table,

#if defined(CONFIG_PM)
	/* Suspend function name */
	.suspend = bt_usb_suspend,

	/* Resume function name */
	.resume = bt_usb_resume,

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
	/* Driver supports autosuspend */
	.supports_autosuspend = 1,
#endif
#endif
};

MODULE_DEVICE_TABLE(usb, bt_usb_table);

#define BT_USB_MAX_ISOC_FRAMES	10

#define BT_USB_INTR_RUNNING	0
#define BT_USB_BULK_RUNNING	1
#define BT_USB_ISOC_RUNNING	2
#define BT_USB_SUSPENDING	3

#define BT_USB_DID_ISO_RESUME	4

/********************************************************
		Global Variables
********************************************************/

int bt_req_fw_nowait;

/********************************************************
		Local Fucntions
********************************************************/
/**
 * @brief Increment tx in flight
 *
 * @param priv          A pointer to bt_private structure
 *
 * @return              0
 */
static int
inc_tx(bt_private *priv)
{
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;
	unsigned long flags;

	spin_lock_irqsave(&card->txlock, flags);
	card->tx_in_flight++;
	spin_unlock_irqrestore(&card->txlock, flags);

	return 0;
}

/**
 * @brief Stops rx traffic
 *
 * @param card          A pointer to usb_card_rec structure
 *
 * @return              N/A
 */
static void
usb_stop_rx_traffic(struct usb_card_rec *card)
{
	usb_kill_anchored_urbs(&card->intr_anchor);
	usb_kill_anchored_urbs(&card->bulk_anchor);
	usb_kill_anchored_urbs(&card->isoc_anchor);
}

/**
 *  @brief   Callback function for Control, Bulk URBs
 *
 *  @param urb     A Pointer to urb structure
 *
 *  @return        N/A
 */
static void
usb_tx_complete(struct urb *urb)
{
	bt_private *priv = (bt_private *)urb->context;
	struct hci_dev *hdev = NULL;
	struct m_dev *m_dev = NULL;
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;

	PRINTM(INFO, "Tx complete: %p urb status %d count %d\n",
	       urb, urb->status, urb->actual_length);

	if (priv->bt_dev.m_dev[BT_SEQ].spec_type == BLUEZ_SPEC)
		hdev = (struct hci_dev *)priv->bt_dev.m_dev[BT_SEQ].dev_pointer;

	if (urb->pipe == usb_sndctrlpipe(card->udev, BT_USB_EP_CMD))
		PRINTM(INFO, "Tx complete: control endpoint\n");
	else if (urb->pipe == usb_sndbulkpipe(card->udev,
					      card->bulk_tx_ep->
					      bEndpointAddress))
		PRINTM(INFO, "Tx complete: bulk endpoint\n");

	if (hdev) {
		if (!urb->status)
			hdev->stat.byte_tx += urb->transfer_buffer_length;
		else
			hdev->stat.err_tx++;
	}

	if (urb->status)
		PRINTM(ERROR, "Tx Urb failed: (%d)\n", urb->status);

	m_dev = &(priv->bt_dev.m_dev[BT_SEQ]);
	bt_interrupt(m_dev);

	spin_lock(&card->txlock);
	card->tx_in_flight--;
	spin_unlock(&card->txlock);

	kfree(urb->setup_packet);
	kfree(urb->transfer_buffer);
	return;
}

/**
 *  @brief   Callback function for Isoc URBs
 *
 *  @param urb     A Pointer to urb structure
 *
 *  @return        N/A
 */
static void
usb_isoc_tx_complete(struct urb *urb)
{
	bt_private *priv = (bt_private *)urb->context;
	struct hci_dev *hdev = NULL;

	PRINTM(INFO, "Tx Isoc complete: %p urb status %d count %d\n",
	       urb, urb->status, urb->actual_length);

	if (priv->bt_dev.m_dev[BT_SEQ].spec_type == BLUEZ_SPEC)
		hdev = (struct hci_dev *)priv->bt_dev.m_dev[BT_SEQ].dev_pointer;

	if (hdev) {
		if (!urb->status)
			hdev->stat.byte_tx += urb->transfer_buffer_length;
		else
			hdev->stat.err_tx++;
	}

	if (urb->status)
		PRINTM(ERROR, "Tx Urb failed: (%d)\n", urb->status);

	kfree(urb->setup_packet);
	kfree(urb->transfer_buffer);
	return;
}

/**
 * @brief This function is used to transfer rx_buf from card to host
 *
 * @param priv          A pointer to bt_private structure
 * @param pkt_type      Packet type
 * @param rx_buf        Rx buffer
 * @param rx_len		Rx length
 *
 * @return              BT_STATUS_SUCCESS/BT_STATUS_FAILURE
 */
static int
usb_card_to_host(bt_private *priv, u32 pkt_type, u8 *rx_buf, u32 rx_len)
{
	int ret = BT_STATUS_SUCCESS;
	struct hci_dev *hdev = NULL;
	struct m_dev *mdev_nfc = &(priv->bt_dev.m_dev[NFC_SEQ]);
	struct nfc_dev *nfc_dev =
		(struct nfc_dev *)priv->bt_dev.m_dev[NFC_SEQ].dev_pointer;
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;
	struct sk_buff *skb = NULL;
	u8 *payload = NULL;
	int buf_len = 0;

	ENTER();

	if (!card) {
		PRINTM(ERROR, "BT: card is NULL!\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	if (priv->bt_dev.m_dev[BT_SEQ].spec_type == BLUEZ_SPEC)
		hdev = (struct hci_dev *)priv->bt_dev.m_dev[BT_SEQ].dev_pointer;
	if (pkt_type < HCI_ACLDATA_PKT || pkt_type > HCI_EVENT_PKT) {
		ret = BT_STATUS_FAILURE;
		goto exit;
	}
	PRINTM(DATA, "BT: USB Rd %s: len=%d type=%d\n", hdev->name, rx_len,
	       pkt_type);
	DBG_HEXDUMP(DAT_D, "BT: USB Rd", rx_buf, rx_len);
	while (rx_len) {
		/* Check Rx length */
		switch (pkt_type) {
		case HCI_EVENT_PKT:
			if (rx_len >= HCI_EVENT_HDR_SIZE) {
				struct hci_event_hdr *h =
					(struct hci_event_hdr *)rx_buf;
				buf_len = HCI_EVENT_HDR_SIZE + h->plen;
			} else
				ret = BT_STATUS_FAILURE;
			break;
		case HCI_ACLDATA_PKT:
			if (rx_len >= HCI_ACL_HDR_SIZE) {
				struct hci_acl_hdr *h =
					(struct hci_acl_hdr *)rx_buf;
				buf_len =
					HCI_ACL_HDR_SIZE +
					__le16_to_cpu(h->dlen);
			} else
				ret = BT_STATUS_FAILURE;
			break;

		case HCI_SCODATA_PKT:
			if (rx_len >= HCI_SCO_HDR_SIZE) {
				struct hci_sco_hdr *h =
					(struct hci_sco_hdr *)rx_buf;
				buf_len = HCI_SCO_HDR_SIZE + h->dlen;
			} else
				ret = BT_STATUS_FAILURE;
			break;
		}

		if (rx_len < buf_len)
			ret = BT_STATUS_FAILURE;

		if (ret == BT_STATUS_FAILURE) {
			PRINTM(ERROR, "BT: Invalid Length =%d!\n", rx_len);
			goto exit;
		}

		/* Allocate skb */
		skb = bt_skb_alloc(buf_len, GFP_ATOMIC);
		if (!skb) {
			PRINTM(WARN, "BT: Failed to allocate skb\n");
			ret = BT_STATUS_FAILURE;
			goto exit;
		}
		payload = skb->data;
		memcpy(payload, rx_buf, buf_len);

		/* Forward the packet up */
		switch (pkt_type) {
		case HCI_ACLDATA_PKT:
			bt_cb(skb)->pkt_type = pkt_type;
			skb_put(skb, buf_len);
			if (*(u16 *) skb->data == 0xffff) {
				skb_queue_tail(&priv->adapter->fwdump_queue,
					       skb);
				wake_up_interruptible(&priv->MainThread.waitQ);
				break;
			}
			bt_recv_frame(priv, skb);
			break;
		case HCI_SCODATA_PKT:
			bt_cb(skb)->pkt_type = pkt_type;
			skb_put(skb, buf_len);
			bt_recv_frame(priv, skb);
			break;
		case HCI_EVENT_PKT:
			/** add EVT Demux */
			bt_cb(skb)->pkt_type = pkt_type;
			skb_put(skb, buf_len);
			if (skb->data[0] != 0xFF) {
				/* NOTE: Unlike other interfaces, for USB
				   event_type= * MRVL_VENDOR_PKT comes as a
				   subpart of HCI_EVENT_PKT * data[0]=0xFF.
				   Hence it needs to be handled separately * by
				   bt_process_event() first. */
				if (BT_STATUS_SUCCESS ==
				    check_evtpkt(priv, skb))
					break;
			}
			switch (skb->data[0]) {
			case 0x0E:
				/** cmd complete */
				if (skb->data[3] == 0x81 &&
				    skb->data[4] == 0xFE) {
					/** NFC cmd complete */
					if (nfc_dev) {
						skb->dev = (void *)mdev_nfc;
						mdev_recv_frame(skb);
						mdev_nfc->stat.byte_rx +=
							buf_len;
					}
					break;
				}
				bt_recv_frame(priv, skb);
				break;
			case 0x0F:
				/** cmd status */
				if (skb->data[4] == 0x81 &&
				    skb->data[5] == 0xFE) {
					/** NFC cmd ststus */
					if (nfc_dev) {
						skb->dev = (void *)mdev_nfc;
						mdev_recv_frame(skb);
						mdev_nfc->stat.byte_rx +=
							buf_len;
					}
					break;
				}
				bt_recv_frame(priv, skb);
				break;
			case 0xFF:
				/** Vendor specific pkt */
				if (skb->data[2] == 0xC0) {
					/** NFC EVT */
					if (nfc_dev) {
						skb->dev = (void *)mdev_nfc;
						mdev_recv_frame(skb);
						mdev_nfc->stat.byte_rx +=
							buf_len;
					}
					break;
				}
				if (BT_STATUS_SUCCESS !=
				    bt_process_event(priv, skb))
					bt_recv_frame(priv, skb);
				break;
			default:
				bt_recv_frame(priv, skb);
				break;
			}
			break;
		default:
			break;
		}
		rx_len -= buf_len;
		rx_buf += buf_len;
	}

exit:
	LEAVE();
	return ret;
}

/**
 *  @brief   reassemble event data packets
 *
 *  @param priv     A Pointer to bt_private structure
 *	@param buffer	Void pointer
 *	@param count	Count
 *
 *  @return        0 for success else fail
 */
static int
btusb_recv_intr(bt_private *priv, void *buffer, int count)
{
	struct sk_buff *skb = NULL;
	struct m_dev *mbt_dev = &priv->bt_dev.m_dev[BT_SEQ];
	int err = 0;

	spin_lock(&mbt_dev->rxlock);
	skb = mbt_dev->evt_skb;

	while (count) {
		int len;

		if (!skb) {
			skb = bt_skb_alloc(HCI_MAX_EVENT_SIZE, GFP_ATOMIC);
			if (!skb) {
				err = -ENOMEM;
				break;
			}

			bt_cb(skb)->pkt_type = HCI_EVENT_PKT;
			bt_cb(skb)->expect = HCI_EVENT_HDR_SIZE;
		}

		len = min_t(uint, bt_cb(skb)->expect, count);
		memcpy(skb_put(skb, len), buffer, len);

		count -= len;
		buffer += len;
		bt_cb(skb)->expect -= len;

		if (skb->len == HCI_EVENT_HDR_SIZE) {
			/* Complete event header */
			bt_cb(skb)->expect = hci_event_hdr (skb)->plen;

			if (skb_tailroom(skb) < bt_cb(skb)->expect) {
				PRINTM(ERROR,
				       "Event data exceeded skb tail room\n");
				kfree_skb(skb);
				skb = NULL;
				err = -EILSEQ;
				break;
			}
		}

		if (bt_cb(skb)->expect == 0) {
			/* Complete frame */
			usb_card_to_host(priv, HCI_EVENT_PKT, skb->data,
					 skb->len);
			kfree_skb(skb);
			skb = NULL;
		}
	}

	mbt_dev->evt_skb = skb;
	spin_unlock(&mbt_dev->rxlock);

	return err;
}

/**
 *  @brief   reassemble ACL data packets
 *
 *  @param priv     A Pointer to bt_private structure
 *	@param buffer	Void pointer
 *	@param count	Count
 *
 *  @return        0 for success else fail
 */
static int
btusb_recv_bulk(bt_private *priv, void *buffer, int count)
{
	struct sk_buff *skb = NULL;
	struct m_dev *mbt_dev = &priv->bt_dev.m_dev[BT_SEQ];
	int err = 0;

	spin_lock(&mbt_dev->rxlock);
	skb = mbt_dev->acl_skb;

	while (count) {
		int len;

		if (!skb) {
			skb = bt_skb_alloc(HCI_MAX_FRAME_SIZE, GFP_ATOMIC);
			if (!skb) {
				err = -ENOMEM;
				break;
			}

			bt_cb(skb)->pkt_type = HCI_ACLDATA_PKT;
			bt_cb(skb)->expect = HCI_ACL_HDR_SIZE;
		}

		len = min_t(uint, bt_cb(skb)->expect, count);
		memcpy(skb_put(skb, len), buffer, len);

		count -= len;
		buffer += len;
		bt_cb(skb)->expect -= len;

		if (skb->len == HCI_ACL_HDR_SIZE) {
			__le16 dlen = hci_acl_hdr(skb)->dlen;

			/* Complete ACL header */
			bt_cb(skb)->expect = __le16_to_cpu(dlen);

			if (skb_tailroom(skb) < bt_cb(skb)->expect) {
				PRINTM(ERROR,
				       "ACL data exceeded skb tail room\n");
				kfree_skb(skb);
				skb = NULL;
				err = -EILSEQ;
				break;
			}
		}

		if (bt_cb(skb)->expect == 0) {
			/* Complete frame */
			usb_card_to_host(priv, HCI_ACLDATA_PKT, skb->data,
					 skb->len);
			kfree_skb(skb);
			skb = NULL;
		}
	}

	mbt_dev->acl_skb = skb;
	spin_unlock(&mbt_dev->rxlock);

	return err;
}

/**
 *  @brief   reassemble SCO data packets
 *
 *  @param priv     A Pointer to bt_private structure
 *	@param buffer	Void pointer
 *	@param count	Count
 *
 *  @return        0 for success else fail
 */
static int
btusb_recv_isoc(bt_private *priv, void *buffer, int count)
{
	struct sk_buff *skb = NULL;
	struct m_dev *mbt_dev = &priv->bt_dev.m_dev[BT_SEQ];
	int err = 0;

	spin_lock(&mbt_dev->rxlock);
	skb = mbt_dev->sco_skb;
	while (count) {
		int len;
		if (!skb) {
			skb = bt_skb_alloc(HCI_MAX_SCO_SIZE, GFP_ATOMIC);
			if (!skb) {
				PRINTM(ERROR, "failed to alloc sco skb\n");
				err = -ENOMEM;
				break;
			}
			bt_cb(skb)->pkt_type = HCI_SCODATA_PKT;
			bt_cb(skb)->expect = HCI_SCO_HDR_SIZE;
		}

		len = min_t(uint, bt_cb(skb)->expect, count);
		memcpy(skb_put(skb, len), buffer, len);

		count -= len;
		buffer += len;
		bt_cb(skb)->expect -= len;

		if (skb->len == HCI_SCO_HDR_SIZE) {
			/* Complete SCO header */
			bt_cb(skb)->expect = hci_sco_hdr(skb)->dlen;
			if (skb_tailroom(skb) < bt_cb(skb)->expect) {
				PRINTM(ERROR,
				       "sco data exceeded skb tail room\n");
				kfree_skb(skb);
				skb = NULL;
				err = -EILSEQ;
				break;
			}
		}

		if (bt_cb(skb)->expect == 0) {
			/* Complete frame */
			usb_card_to_host(priv, HCI_SCODATA_PKT, skb->data,
					 skb->len);
			kfree_skb(skb);
			skb = NULL;
		}
	}

	mbt_dev->sco_skb = skb;
	spin_unlock(&mbt_dev->rxlock);
	return err;
}

/**
 *  @brief   Callback function for Interrupt IN URB (Event)
 *
 *  @param urb     A Pointer to urb structure
 *
 *  @return        0 for success else fail
 */
static void
usb_intr_rx_complete(struct urb *urb)
{
	int err = 0;
	bt_private *priv = (bt_private *)urb->context;
	struct hci_dev *hdev = NULL;
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;

	PRINTM(INFO, "Intr Rx complete: urb %p status %d count %d\n",
	       urb, urb->status, urb->actual_length);

	if (priv->bt_dev.m_dev[BT_SEQ].spec_type == BLUEZ_SPEC)
		hdev = (struct hci_dev *)priv->bt_dev.m_dev[BT_SEQ].dev_pointer;

	if (urb->status == 0) {

		// DBG_HEXDUMP(DAT_D, "Intr Rx: ", urb->transfer_buffer,
		// urb->actual_length);
		err = btusb_recv_intr(priv, urb->transfer_buffer,
				      urb->actual_length);
		if (err < 0) {
			PRINTM(ERROR, "Corrupted event packet: %d\n", err);
			if (hdev)
				hdev->stat.err_rx++;
		}
	} else if (urb->status == -ENOENT) {
		/* Avoid suspend failed when usb_kill_urb */
		return;
	}

	if (!test_bit(BT_USB_INTR_RUNNING, &card->flags))
		return;

	usb_mark_last_busy(card->udev);
	usb_anchor_urb(urb, &card->intr_anchor);

	/* Resubmit the URB */
	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		PRINTM(ERROR, "Intr Rx urb %p failed to resubmit (%d)\n",
		       urb, err);
		usb_unanchor_urb(urb);
	}
	return;
}

/**
 *  @brief   Callback function for Bulk IN URB (ACL Data)
 *
 *  @param urb     A Pointer to urb structure
 *
 *  @return        0 for success else fail
 */
static void
usb_bulk_rx_complete(struct urb *urb)
{
	int err = 0;
	bt_private *priv = (bt_private *)urb->context;
	struct hci_dev *hdev = NULL;
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;

	PRINTM(INFO, "Bulk RX complete: urb %p status %d count %d\n",
	       urb, urb->status, urb->actual_length);

	if (priv->bt_dev.m_dev[BT_SEQ].spec_type == BLUEZ_SPEC)
		hdev = (struct hci_dev *)priv->bt_dev.m_dev[BT_SEQ].dev_pointer;

	if (urb->status == 0) {
		err = btusb_recv_bulk(priv, urb->transfer_buffer,
				      urb->actual_length);
		if (err < 0) {
			PRINTM(ERROR, "Corrupted ACL packet: %d\n", err);
			if (hdev)
				hdev->stat.err_rx++;
		}
	} else if (urb->status == -ENOENT) {
		/* Avoid suspend failed when usb_kill_urb */
		return;
	}

	if (!test_bit(BT_USB_BULK_RUNNING, &card->flags))
		return;

	usb_anchor_urb(urb, &card->bulk_anchor);
	usb_mark_last_busy(card->udev);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		PRINTM(ERROR, "urb %p failed to resubmit (%d)\n", urb, err);
		usb_unanchor_urb(urb);
	}
	return;
}

/**
 *  @brief   	   This function submits bulk URB (Async URB completion)
 *  @param urb     A Pointer to urb structure
 *
 *  @return        0 for success else fail
 */
static int
usb_submit_bt_bulk_urb(bt_private *priv, gfp_t mem_flags)
{
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;
	struct urb *urb;
	int err, size = ALLOC_BUF_SIZE;
	unsigned int pipe;
	unsigned char *buf;

	ENTER();

	if (!card->bulk_rx_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, mem_flags);
	if (!urb)
		return -ENOMEM;

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}
	pipe = usb_rcvbulkpipe(card->udev, card->bulk_rx_ep->bEndpointAddress);

	usb_fill_bulk_urb(urb, card->udev, pipe,
			  buf, size, usb_bulk_rx_complete, priv);

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_mark_last_busy(card->udev);
	usb_anchor_urb(urb, &card->bulk_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		PRINTM(ERROR, "bulk urb %p submission failed (%d)", urb, err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	LEAVE();
	return err;
}

/**
 *  @brief   	   This function submits Int URB (Async URB completion)
 *  @param urb     A Pointer to urb structure
 *
 *  @return        0 for success else fail
 */
static int
usb_submit_bt_intr_urb(bt_private *priv, gfp_t mem_flags)
{
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;
	struct urb *urb;
	int err, size = HCI_MAX_EVENT_SIZE;
	unsigned char *buf;
	unsigned int pipe;

	ENTER();

	if (!card->intr_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, mem_flags);
	if (!urb)
		return -ENOMEM;

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	pipe = usb_rcvintpipe(card->udev, card->intr_ep->bEndpointAddress);

	usb_fill_int_urb(urb, card->udev, pipe, buf, size,
			 usb_intr_rx_complete, priv, card->intr_ep->bInterval);
	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_anchor_urb(urb, &card->intr_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		PRINTM(ERROR, "intr urb %p submission failed (%d)", urb, err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	LEAVE();
	return err;
}

/**
 *  @brief   	   Based on which Alternate Setting needs to be used,
 *  			   set the Isoc interface
 *
 *  @param priv       A Pointer to bt_private structure
 *	@param altsetting Argument
 *  @return         0 for success else fail
 */
static int
usb_set_isoc_interface(bt_private *priv, int altsetting)
{
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;
	struct usb_interface *intf = card->isoc;
	struct usb_endpoint_descriptor *endpoint;
	int i, err;

	ENTER();

	if (!card->isoc) {
		LEAVE();
		return -ENODEV;
	}

	err = usb_set_interface(card->udev, 1, altsetting);
	if (err < 0) {
		PRINTM(ERROR, "Setting isoc interface failed (%d)\n", err);
		LEAVE();
		return err;
	}

	card->isoc_altsetting = altsetting;
	card->isoc_tx_ep = NULL;
	card->isoc_rx_ep = NULL;

	for (i = 0; i < intf->cur_altsetting->desc.bNumEndpoints; i++) {
		endpoint = &intf->cur_altsetting->endpoint[i].desc;

		if (!card->isoc_tx_ep && usb_endpoint_is_isoc_out(endpoint) &&
		    usb_endpoint_num(endpoint) == BT_USB_EP_SCO_DATA) {
			/* We found a Isoc out sync_data endpoint */
			PRINTM(INFO, "Isoc OUT: max pkt size = %d, addr = %d\n",
			       endpoint->wMaxPacketSize,
			       endpoint->bEndpointAddress);
			card->isoc_tx_ep = endpoint;
			continue;
		}

		if (!card->isoc_rx_ep && usb_endpoint_is_isoc_in(endpoint) &&
		    usb_endpoint_num(endpoint) == BT_USB_EP_SCO_DATA) {
			/* We found a Isoc in sync_data endpoint */
			PRINTM(INFO, "Isoc IN: max pkt size = %d, addr = %d\n",
			       endpoint->wMaxPacketSize,
			       endpoint->bEndpointAddress);
			card->isoc_rx_ep = endpoint;
			continue;
		}
	}

	if (!card->isoc_tx_ep || !card->isoc_rx_ep) {
		PRINTM(ERROR, "invalid SCO endpoints");
		LEAVE();
		return -ENODEV;
	}

	LEAVE();
	return 0;
}

/**
 *  @brief  Callback function for Isoc IN URB (SCO Data)
 *
 *  @param urb       A Pointer to urb structure
 *  @return         0 for success else fail
 */
static void
usb_isoc_rx_complete(struct urb *urb)
{
	int err = 0, i;
	bt_private *priv = (bt_private *)urb->context;
	struct hci_dev *hdev = NULL;
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;

	PRINTM(INFO, "Isoc Rx complete: urb %p status %d count %d\n",
	       urb, urb->status, urb->actual_length);
	if (priv->bt_dev.m_dev[BT_SEQ].spec_type == BLUEZ_SPEC)
		hdev = (struct hci_dev *)priv->bt_dev.m_dev[BT_SEQ].dev_pointer;

	if (urb->status == 0 && urb->actual_length > 0) {
		for (i = 0; i < urb->number_of_packets; i++) {
			unsigned int offset = urb->iso_frame_desc[i].offset;
			unsigned int length =
				urb->iso_frame_desc[i].actual_length;

			if (urb->iso_frame_desc[i].status)
				continue;
			err = btusb_recv_isoc(priv,
					      urb->transfer_buffer + offset,
					      length);
			if (err < 0) {
				PRINTM(ERROR, "Corrupted SCO packet: %d\n",
				       err);
				if (hdev)
					hdev->stat.err_rx++;
			}
		}
	} else if (urb->status == -ENOENT) {
		/* Avoid suspend failed when usb_kill_urb */
		return;
	}

	if (!test_bit(BT_USB_ISOC_RUNNING, &card->flags))
		return;

	usb_anchor_urb(urb, &card->isoc_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		PRINTM(ERROR, "isoc urb %p failed to resubmit (%d)\n",
		       urb, err);
		usb_unanchor_urb(urb);
	}
	return;
}

/**
 *  @brief  	This function submits isoc URB (Async URB completion)
 *
 *  @param priv       A Pointer to bt_private structure
 *  @param mem_flags  Memory flags
 *  @return           0 for success else fail
 */
static int
usb_submit_bt_isoc_urb(bt_private *priv, gfp_t mem_flags)
{
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;
	struct urb *urb;
	int err, size;
	int len;
	int mtu;
	int i, offset = 0;
	unsigned char *buf;
	unsigned int pipe;

	ENTER();

	urb = usb_alloc_urb(BT_USB_MAX_ISOC_FRAMES, GFP_ATOMIC);
	if (!urb)
		return -ENOMEM;

	size = le16_to_cpu(card->isoc_rx_ep->wMaxPacketSize) *
		BT_USB_MAX_ISOC_FRAMES;

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}
	pipe = usb_rcvisocpipe(card->udev, card->isoc_rx_ep->bEndpointAddress);

	urb->dev = card->udev;
	urb->pipe = pipe;
	urb->context = priv;
	urb->complete = usb_isoc_rx_complete;
	urb->interval = card->isoc_rx_ep->bInterval;

	urb->transfer_flags = URB_FREE_BUFFER | URB_ISO_ASAP;
	urb->transfer_buffer = buf;
	urb->transfer_buffer_length = size;

	len = size;
	mtu = le16_to_cpu(card->isoc_rx_ep->wMaxPacketSize);
	for (i = 0; i < BT_USB_MAX_ISOC_FRAMES && len >= mtu;
	     i++, offset += mtu, len -= mtu) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = mtu;
	}

	if (len && i < BT_USB_MAX_ISOC_FRAMES) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = len;
		i++;
	}

	urb->number_of_packets = i;

	usb_anchor_urb(urb, &card->isoc_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		PRINTM(ERROR, "isoc urb %p submission failed (%d)", urb, err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	LEAVE();
	return err;
}

/* TODO: Instead of BlueZ, there is no hook for char dev to notify our driver
 * about SCO connection num's change, so set the isoc interface and submit rx
 * isoc urb immediately when bulk & intr rx urbs are submitted.
 */

/**
 *  @brief  	This function sets isoc
 *
 *  @param priv       A Pointer to bt_private structure
 *
 *  @return           N/A
 */
static void
usb_set_isoc_if(bt_private *priv)
{
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;
	int err;
	struct hci_dev *hdev = NULL;

	ENTER();

	if (priv->bt_dev.m_dev[BT_SEQ].spec_type == BLUEZ_SPEC)
		hdev = (struct hci_dev *)priv->bt_dev.m_dev[BT_SEQ].dev_pointer;

	if (hdev) {
		if (hdev->conn_hash.sco_num > 0) {
			if (!test_bit(BT_USB_DID_ISO_RESUME, &card->flags)) {
				err = usb_autopm_get_interface(card->
							       isoc ? card->
							       isoc : card->
							       intf);
				if (err < 0) {

					clear_bit(BT_USB_ISOC_RUNNING,
						  &card->flags);
					usb_kill_anchored_urbs(&card->
							       isoc_anchor);
					LEAVE();
					return;
				}

				set_bit(BT_USB_DID_ISO_RESUME, &card->flags);
			}
			if (card->isoc_altsetting != 2) {
				clear_bit(BT_USB_ISOC_RUNNING, &card->flags);
				usb_kill_anchored_urbs(&card->isoc_anchor);

				if (usb_set_isoc_interface(priv, 2) < 0) {
					LEAVE();
					return;
				}
			}

			if (!test_and_set_bit
			    (BT_USB_ISOC_RUNNING, &card->flags)) {
				if (usb_submit_bt_isoc_urb(priv, GFP_KERNEL) <
				    0)
					clear_bit(BT_USB_ISOC_RUNNING,
						  &card->flags);
				else
					usb_submit_bt_isoc_urb(priv,
							       GFP_KERNEL);
			}
		} else {
			clear_bit(BT_USB_ISOC_RUNNING, &card->flags);
			usb_kill_anchored_urbs(&card->isoc_anchor);

			usb_set_isoc_interface(priv, 0);
			if (test_bit(BT_USB_DID_ISO_RESUME, &card->flags)) {
				clear_bit(BT_USB_DID_ISO_RESUME, &card->flags);
				usb_autopm_put_interface(card->isoc);
			}
		}
	}
	LEAVE();
}

/**
 *  @brief  	This function unsets isoc
 *
 *  @param priv       A Pointer to bt_private structure
 *
 *  @return           N/A
 */
static void
usb_unset_isoc_if(bt_private *priv)
{
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;
	struct hci_dev *hdev = NULL;

	ENTER();

	if (priv->bt_dev.m_dev[BT_SEQ].spec_type == BLUEZ_SPEC)
		hdev = (struct hci_dev *)priv->bt_dev.m_dev[BT_SEQ].dev_pointer;

	if (hdev) {
		card->sco_num = 0;
		clear_bit(BT_USB_ISOC_RUNNING, &card->flags);
		usb_kill_anchored_urbs(&card->isoc_anchor);

		usb_set_isoc_interface(priv, 0);
		if (test_and_clear_bit(BT_USB_DID_ISO_RESUME, &card->flags))
			usb_autopm_put_interface(card->isoc ? card->
						 isoc : card->intf);
	}
	LEAVE();
}

/**
 *  @brief  	USB isoc work
 *
 *  @param work       A Pointer to work_struct structure
 *
 *  @return           N/A
 */
static void
usb_isoc_work(struct work_struct *work)
{
	struct usb_card_rec *card =
		container_of(work, struct usb_card_rec, work);

	ENTER();
	usb_set_isoc_if((bt_private *)card->priv);
	LEAVE();
}

/**
 *  @brief Sets the configuration values
 *
 *  @param intf		Pointer to usb_interface
 *  @param id		Pointer to usb_device_id
 *
 *  @return	Address of variable usb_cardp, error code otherwise
 */
static int
bt_usb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *udev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i, ret = 0;
	struct usb_card_rec *usb_cardp = NULL;
	bt_private *priv = NULL;

	PRINTM(MSG, "bt_usb_probe: intf %p id %p", intf, id);

	/* interface numbers are hardcoded in the spec */
	if (intf->cur_altsetting->desc.bInterfaceNumber != 0)
		return -ENODEV;

	usb_cardp = kzalloc(sizeof(struct usb_card_rec), GFP_KERNEL);
	if (!usb_cardp)
		return -ENOMEM;

	udev = interface_to_usbdev(intf);

	/* Check probe is for our device */
	for (i = 0; bt_usb_table[i].idVendor; i++) {

		if (udev->descriptor.idVendor == bt_usb_table[i].idVendor &&
		    udev->descriptor.idProduct == bt_usb_table[i].idProduct) {

			PRINTM(MSG, "VID/PID = %X/%X, Boot2 version = %X\n",
			       udev->descriptor.idVendor,
			       udev->descriptor.idProduct,
			       udev->descriptor.bcdDevice);

			/* Update boot state */
			switch (udev->descriptor.idProduct) {
			case USB8897_PID_2:
				usb_cardp->boot_state = USB_FW_READY;
				break;
			default:
				usb_cardp->boot_state = USB_FW_DNLD;
				break;
			}

			/* Update card type */
			switch (udev->descriptor.idProduct) {
			case USB8897_PID_2:
				usb_cardp->card_type = CARD_TYPE_USB8897;
				break;
			default:
				PRINTM(ERROR, "Invalid card type detected\n");
				break;
			}
			break;
		}
	}

	if (bt_usb_table[i].idVendor) {

		usb_cardp->udev = udev;
		usb_cardp->intf = intf;
		iface_desc = intf->cur_altsetting;

		PRINTM(INFO, "bcdUSB = 0x%X bDeviceClass = 0x%X"
		       " bDeviceSubClass = 0x%X, bDeviceProtocol = 0x%X\n",
		       udev->descriptor.bcdUSB,
		       udev->descriptor.bDeviceClass,
		       udev->descriptor.bDeviceSubClass,
		       udev->descriptor.bDeviceProtocol);

		/* BT Commands will use USB's control endpoint (Ept-0) */
		PRINTM(INFO, "CTRL IN/OUT: max pkt size = %d, addr = %d\n",
		       udev->descriptor.bMaxPacketSize0, BT_USB_EP_CMD);

		/* Extract Other Endpoints */
		for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
			endpoint = &iface_desc->endpoint[i].desc;

			if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
			    &&
			    ((endpoint->
			      bEndpointAddress & USB_ENDPOINT_NUMBER_MASK) ==
			     BT_USB_EP_CMD_EVENT) &&
			    ((endpoint->
			      bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
			     USB_ENDPOINT_XFER_BULK)) {
				/* We found a bulk in command/event endpoint */
				PRINTM(INFO,
				       "Bulk IN: max packet size = %d, address = %d\n",
				       endpoint->wMaxPacketSize,
				       endpoint->bEndpointAddress);
				usb_cardp->rx_cmd_ep =
					(endpoint->
					 bEndpointAddress &
					 USB_ENDPOINT_NUMBER_MASK);
				continue;
			}
			if (((endpoint->
			      bEndpointAddress & USB_ENDPOINT_DIR_MASK) ==
			     USB_DIR_OUT) &&
			    ((endpoint->
			      bEndpointAddress & USB_ENDPOINT_NUMBER_MASK) ==
			     BT_USB_EP_CMD_EVENT) &&
			    ((endpoint->
			      bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
			     USB_ENDPOINT_XFER_BULK)) {
				/* We found a bulk out command/event endpoint */
				PRINTM(INFO,
				       "Bulk OUT: max packet size = %d, address = %d\n",
				       endpoint->wMaxPacketSize,
				       endpoint->bEndpointAddress);
				usb_cardp->tx_cmd_ep =
					endpoint->bEndpointAddress;
				usb_cardp->bulk_out_maxpktsize =
					endpoint->wMaxPacketSize;
				continue;
			}
			if (!usb_cardp->intr_ep &&
			    usb_endpoint_is_int_in(endpoint) &&
			    usb_endpoint_num(endpoint) == BT_USB_EP_EVENT) {

				/* We found a interrupt in event endpoint */
				PRINTM(INFO, "INT IN: max pkt size = %d, "
				       "addr = %d\n",
				       endpoint->wMaxPacketSize,
				       endpoint->bEndpointAddress);
				usb_cardp->intr_ep = endpoint;
				continue;
			}

			if (!usb_cardp->bulk_rx_ep &&
			    usb_endpoint_is_bulk_in(endpoint) &&
			    usb_endpoint_num(endpoint) == BT_USB_EP_ACL_DATA) {

				/* We found a bulk in data endpoint */
				PRINTM(INFO, "Bulk IN: max pkt size = %d, "
				       "addr = %d\n",
				       endpoint->wMaxPacketSize,
				       endpoint->bEndpointAddress);
				usb_cardp->bulk_rx_ep = endpoint;
				continue;
			}

			if (!usb_cardp->bulk_tx_ep &&
			    usb_endpoint_is_bulk_out(endpoint) &&
			    usb_endpoint_num(endpoint) == BT_USB_EP_ACL_DATA) {

				/* We found a bulk out data endpoint */
				PRINTM(INFO, "Bulk OUT: max pkt size = %d, "
				       "addr = %d\n",
				       endpoint->wMaxPacketSize,
				       endpoint->bEndpointAddress);
				usb_cardp->bulk_tx_ep = endpoint;
				continue;
			}
		}

		if (!usb_cardp->intr_ep || !usb_cardp->bulk_tx_ep ||
		    !usb_cardp->bulk_rx_ep) {
			ret = -ENODEV;
			goto error;
		}

		usb_cardp->cmdreq_type = USB_TYPE_CLASS;

		spin_lock_init(&usb_cardp->txlock);
		spin_lock_init(&usb_cardp->rxlock);

		INIT_WORK(&usb_cardp->work, usb_isoc_work);

		init_usb_anchor(&usb_cardp->tx_anchor);
		init_usb_anchor(&usb_cardp->intr_anchor);
		init_usb_anchor(&usb_cardp->bulk_anchor);
		init_usb_anchor(&usb_cardp->isoc_anchor);

		/* Interface numbers are hardcoded in the specification */
		usb_cardp->isoc = usb_ifnum_to_if(udev, 1);

		if (usb_cardp->isoc) {

			ret = usb_driver_claim_interface(&bt_usb_driver,
							 usb_cardp->isoc,
							 usb_cardp);
			if (ret < 0)
				goto error;
			PRINTM(INFO, "bt_usb_probe: isoc intf %p id %p",
			       usb_cardp->isoc, id);
		}

		usb_set_intfdata(intf, usb_cardp);

		/* At this point bt_add_card() will be called */
		priv = bt_add_card(usb_cardp);
		usb_cardp->priv = (void *)priv;
		if (!priv) {
			PRINTM(ERROR, "bt_add_card failed\n");
			goto error;
		}
		usb_get_dev(udev);
		return 0;
	} else {
		PRINTM(INFO, "Discard the Probe request\n");
		PRINTM(INFO, "VID = 0x%X PID = 0x%X\n",
		       udev->descriptor.idVendor, udev->descriptor.idProduct);
	}

error:
	if (ret != (-ENODEV))
		ret = -ENXIO;
	kfree(usb_cardp);
	return ret;
}

/**
 *  @brief Free resource and cleanup
 *
 *  @param intf		Pointer to usb_interface
 *
 *  @return		N/A
 */
static void
bt_usb_disconnect(struct usb_interface *intf)
{
	struct usb_card_rec *cardp = usb_get_intfdata(intf);

	PRINTM(MSG, "bt_usb_disconnect: intf %p\n", intf);

	if (!cardp) {
		PRINTM(INFO, "Card is not valid\n");
		return;
	}

	PRINTM(INFO, "Call remove card\n");
	bt_remove_card(cardp->priv);

	PRINTM(INFO, "Call USB cleanup routines\n");
	usb_set_intfdata(cardp->intf, NULL);
	usb_put_dev(interface_to_usbdev(intf));
	if (cardp->isoc)
		usb_set_intfdata(cardp->isoc, NULL);

	if (intf == cardp->isoc)
		usb_driver_release_interface(&bt_usb_driver, cardp->intf);
	else if (cardp->isoc)
		usb_driver_release_interface(&bt_usb_driver, cardp->isoc);

	kfree(cardp);
	return;
}

#ifdef CONFIG_PM
/**
 *  @brief Handle suspend
 *
 *  @param intf		Pointer to usb_interface
 *  @param message	Pointer to pm_message_t structure
 *
 *  @return		BT_STATUS_SUCCESS
 */
static int
bt_usb_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct usb_card_rec *cardp = usb_get_intfdata(intf);

	ENTER();

	if (!cardp) {
		PRINTM(ERROR, "usb_card_rec structure is not valid\n");
		LEAVE();
		return BT_STATUS_FAILURE;
	}

	PRINTM(CMD, "ready to suspend BT USB\n");
	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief Handle resume
 *
 *  @param intf		Pointer to usb_interface
 *
 *  @return		BT_STATUS_SUCCESS
 */
static int
bt_usb_resume(struct usb_interface *intf)
{
	int ret = BT_STATUS_SUCCESS;
	struct usb_card_rec *cardp = usb_get_intfdata(intf);
	ENTER();

	if (!cardp) {
		PRINTM(ERROR, "usb_card_rec structure is not valid\n");
		LEAVE();
		return BT_STATUS_FAILURE;
	}

	LEAVE();
	return ret;
}
#endif

/********************************************************
		Global Fucntions
********************************************************/
/**
 *  @brief  Free fragments
 *
 *  @param priv		Pointer to bt_private structure
 *
 *  @return			N/A
 */
void
usb_free_frags(bt_private *priv)
{
	unsigned long flags;
	struct m_dev *mbt_dev = &priv->bt_dev.m_dev[BT_SEQ];

	spin_lock_irqsave(&mbt_dev->rxlock, flags);

	kfree_skb(mbt_dev->evt_skb);
	mbt_dev->evt_skb = NULL;

	kfree_skb(mbt_dev->acl_skb);
	mbt_dev->acl_skb = NULL;

	kfree_skb(mbt_dev->sco_skb);
	mbt_dev->sco_skb = NULL;

	spin_unlock_irqrestore(&mbt_dev->rxlock, flags);
}

/**
 *  @brief  Flush USB
 *
 *  @param priv		Pointer to bt_private structure
 *
 *  @return			N/A
 */
int
usb_flush(bt_private *priv)
{
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;

	if (card)
		usb_kill_anchored_urbs(&card->tx_anchor);

	usb_free_frags(priv);
	return 0;
}

/**
 *  @brief  Notify events
 *
 *  @param priv		Pointer to bt_private structure
 *  @param evt		Event
 *
 *  @return			N/A
 */
void
usb_notify(bt_private *priv, unsigned int evt)
{
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;
	struct hci_dev *hdev = NULL;

	ENTER();

	if (priv->bt_dev.m_dev[BT_SEQ].spec_type == BLUEZ_SPEC)
		hdev = (struct hci_dev *)priv->bt_dev.m_dev[BT_SEQ].dev_pointer;

	if (hdev) {

		PRINTM(INFO, "%s evt %d", hdev->name, evt);
		PRINTM(INFO, "hdev->conn_hash.sco_num=%d, card->sco_num=%d",
		       hdev->conn_hash.sco_num, card->sco_num);

		if (hdev->conn_hash.sco_num != card->sco_num) {
			card->sco_num = hdev->conn_hash.sco_num;
			schedule_work(&card->work);
		}
	}
	LEAVE();
}

/**
 *  @brief  Sets usb isoc based on arg
 *
 *  @param priv		Pointer to bt_private structure
 *  @param arg		Argument
 *
 *  @return			N/A
 */
void
usb_char_notify(bt_private *priv, unsigned int arg)
{
	ENTER();
	if (!priv) {
		LEAVE();
		return;
	}
	if (arg == 1)
		usb_set_isoc_if(priv);
	else
		usb_unset_isoc_if(priv);
	LEAVE();
}

/**
 *  @brief bt driver call this function to register to bus driver
 *  This function will be used to register bt driver's add/remove
 *  callback function.
 *
 *  @return	BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int *
sbi_register(void)
{
	int *ret;
	ENTER();

	PRINTM(MSG, "Marvell Bluetooth USB driver\n");

	/*
	 * API registers the Marvell USB driver
	 * to the USB system
	 */
	if (usb_register(&bt_usb_driver)) {
		PRINTM(FATAL, "USB Driver Registration Failed\n");
		return NULL;
	} else {
		ret = (int *)1;
	}

	LEAVE();
	return ret;
}

/**
 *  @brief bt driver call this function to unregister to bus driver
 *  This function will be used to unregister bt driver.
 *
 *  @return	NA
 */
void
sbi_unregister(void)
{
	ENTER();

	/* API unregisters the driver from USB subsystem */
	usb_deregister(&bt_usb_driver);

	LEAVE();
	return;
}

/**
 *  @brief bt driver calls this function to register the device
 *
 *  @param priv	A pointer to bt_private structure
 *  @return	BT_STATUS_SUCCESS
 */
int
sbi_register_dev(bt_private *priv)
{
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;
	int ret = BT_STATUS_SUCCESS;

	ENTER();

	priv->hotplug_device = &card->udev->dev;
	strncpy(priv->bt_dev.name, "bt_usb0", sizeof(priv->bt_dev.name));

	ret = usb_autopm_get_interface(card->intf);
	if (ret < 0) {
		LEAVE();
		return ret;
	}

	card->intf->needs_remote_wakeup = 1;

	if (test_and_set_bit(BT_USB_INTR_RUNNING, &card->flags))
		goto done;

	/* Submit Rx Interrupt URB (For Events) */
	ret = usb_submit_bt_intr_urb(priv, GFP_ATOMIC);
	if (ret < 0)
		goto failed;

	/* Submit Rx Bulk URB (For ACL Data) */
	ret = usb_submit_bt_bulk_urb(priv, GFP_ATOMIC);
	if (ret < 0) {
		usb_kill_anchored_urbs(&card->intr_anchor);
		goto failed;
	}

	set_bit(BT_USB_BULK_RUNNING, &card->flags);

done:
	usb_autopm_put_interface(card->intf);
	LEAVE();
	return BT_STATUS_SUCCESS;

failed:
	clear_bit(BT_USB_INTR_RUNNING, &card->flags);
	usb_autopm_put_interface(card->intf);
	LEAVE();
	return ret;
}

/**
 *  @brief bt driver calls this function to unregister the device
 *
 *  @param priv		A pointer to bt_private structure
 *  @return		BT_STATUS_SUCCESS
 */
int
sbi_unregister_dev(bt_private *priv)
{
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;
	int err;

	ENTER();

	clear_bit(BT_USB_ISOC_RUNNING, &card->flags);
	clear_bit(BT_USB_BULK_RUNNING, &card->flags);
	clear_bit(BT_USB_INTR_RUNNING, &card->flags);

	cancel_work_sync(&card->work);

	usb_stop_rx_traffic(card);
	usb_kill_anchored_urbs(&card->tx_anchor);

	err = usb_autopm_get_interface(card->intf);
	if (err < 0) {
		LEAVE();
		return err;
	}

	card->intf->needs_remote_wakeup = 0;
	usb_autopm_put_interface(card->intf);

	LEAVE();
	return BT_STATUS_SUCCESS;
}

/**
 *  @brief This function sends data to the card.
 *
 *  @param priv		A pointer to bt_private structure
 *  @param skb		A pointer to sk_buff structure
 *
 *  @return		BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sbi_host_to_card(bt_private *priv, struct sk_buff *skb)
{
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;
	struct usb_ctrlrequest *req = NULL;
	struct urb *urb = NULL;
	unsigned int pipe;
	int len, mtu, i, offset = 0;
	u8 *buf = NULL;
	struct hci_dev *hdev = NULL;

	ENTER();

	if (priv->bt_dev.m_dev[BT_SEQ].spec_type == BLUEZ_SPEC)
		hdev = (struct hci_dev *)priv->bt_dev.m_dev[BT_SEQ].dev_pointer;

	buf = kmalloc(skb->len, GFP_ATOMIC);
	if (!buf) {
		LEAVE();
		return -ENOMEM;
	}
	memcpy(buf, skb->data, skb->len);

	switch (bt_cb(skb)->pkt_type) {

	case HCI_COMMAND_PKT:
	case MRVL_VENDOR_PKT:
		urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!urb)
			goto error;

		req = kmalloc(sizeof(*req), GFP_ATOMIC);
		if (!req)
			goto error;

		req->bRequestType = card->cmdreq_type;
		req->bRequest = 0;
		req->wIndex = 0;
		req->wValue = 0;
		req->wLength = __cpu_to_le16(skb->len);

		pipe = usb_sndctrlpipe(card->udev, BT_USB_EP_CMD);

		usb_fill_control_urb(urb, card->udev, pipe, (void *)req,
				     buf, skb->len, usb_tx_complete, priv);

		if (hdev)
			hdev->stat.cmd_tx++;
		break;

	case HCI_ACLDATA_PKT:

		if (!card->bulk_tx_ep)
			goto error;

		urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!urb)
			goto error;

		pipe = usb_sndbulkpipe(card->udev,
				       card->bulk_tx_ep->bEndpointAddress);

		usb_fill_bulk_urb(urb, card->udev, pipe,
				  buf, skb->len, usb_tx_complete, priv);

		if (hdev)
			hdev->stat.acl_tx++;
		break;

	case HCI_SCODATA_PKT:

		if (!card->isoc_tx_ep)
			goto error;

		if (hdev && (hdev->conn_hash.sco_num < 1)) {
			PRINTM(ERROR, "hdev: sco_num test failed\n");
			goto error;
		}

		urb = usb_alloc_urb(BT_USB_MAX_ISOC_FRAMES, GFP_ATOMIC);
		if (!urb)
			goto error;

		pipe = usb_sndisocpipe(card->udev,
				       card->isoc_tx_ep->bEndpointAddress);
		urb->dev = card->udev;
		urb->pipe = pipe;
		urb->context = priv;
		urb->complete = usb_isoc_tx_complete;
		urb->interval = card->isoc_tx_ep->bInterval;

		urb->transfer_flags = URB_ISO_ASAP;
		urb->transfer_buffer = buf;
		urb->transfer_buffer_length = skb->len;

		len = skb->len;
		mtu = le16_to_cpu(card->isoc_tx_ep->wMaxPacketSize);
		for (i = 0; i < BT_USB_MAX_ISOC_FRAMES && len >= mtu;
		     i++, offset += mtu, len -= mtu) {
			urb->iso_frame_desc[i].offset = offset;
			urb->iso_frame_desc[i].length = mtu;
		}

		if (len && i < BT_USB_MAX_ISOC_FRAMES) {
			urb->iso_frame_desc[i].offset = offset;
			urb->iso_frame_desc[i].length = len;
			i++;
		}

		if (hdev)
			hdev->stat.sco_tx++;
		goto skip_waking;

	default:
		PRINTM(MSG, "Unknown Packet type!\n");
		goto error;
	}

	if (inc_tx(priv)) {

		/* Currently no deferring of work allowed, return error */
		PRINTM(INFO, "inc_tx() failed\n");
		goto error;
	}

skip_waking:
	PRINTM(DATA, "BT: USB Wr: len=%d type=%d\n", skb->len,
	       bt_cb(skb)->pkt_type);
	DBG_HEXDUMP(DAT_D, "BT: USB Wr", skb->data, skb->len);

	usb_anchor_urb(urb, &card->tx_anchor);

	if (usb_submit_urb(urb, GFP_ATOMIC) < 0) {
		PRINTM(ERROR, "sbi_host_to_card: urb %p submission failed",
		       urb);
		kfree(urb->setup_packet);
		usb_unanchor_urb(urb);
		goto error;
	} else {
		usb_mark_last_busy(card->udev);
	}

	PRINTM(INFO, "Tx submit: %p urb %d pkt_type\n",
	       urb, bt_cb(skb)->pkt_type);

	usb_free_urb(urb);
	LEAVE();
	return BT_STATUS_SUCCESS;

error:
	PRINTM(INFO, "Tx submit failed: %p urb %d pkt_type\n",
	       urb, bt_cb(skb)->pkt_type);
	kfree(buf);
	usb_free_urb(urb);
	LEAVE();
	return BT_STATUS_FAILURE;
}

/**
 *  @brief This function initializes firmware
 *
 *  @param priv		A pointer to bt_private structure
 *  @return		BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sbi_download_fw(bt_private *priv)
{
	int ret = BT_STATUS_SUCCESS;
	struct usb_card_rec *card = (struct usb_card_rec *)priv->bt_dev.card;

	ENTER();

	if (!card) {
		PRINTM(ERROR, "BT: card  is NULL!\n");
		ret = BT_STATUS_FAILURE;
		goto exit;
	}

	if (TRUE && (card->boot_state != USB_FW_READY)) {
		PRINTM(MSG, "FW is not Active, Needs to be downloaded\n");
		goto exit;
	} else {
		PRINTM(MSG, "FW is Active\n");
		/* Set it to NULL, no downloading firmware to card */
		priv->firmware = NULL;
		if (BT_STATUS_FAILURE == sbi_register_conf_dpc(priv)) {
			PRINTM(ERROR,
			       "BT: sbi_register_conf_dpc failed. Terminating d/w\n");
			ret = BT_STATUS_FAILURE;
			goto exit;
		}
	}

exit:
	LEAVE();
	return ret;
}

/**
 *  @brief configures hardware to quit deep sleep state
 *
 *  @param priv		A pointer to bt_private structure
 *  @return		BT_STATUS_SUCCESS or BT_STATUS_FAILURE
 */
int
sbi_wakeup_firmware(bt_private *priv)
{
	int ret = BT_STATUS_SUCCESS;
	ENTER();

	/* No explicit wakeup for USB interface */
	priv->adapter->ps_state = PS_AWAKE;

	LEAVE();
	return ret;
}
