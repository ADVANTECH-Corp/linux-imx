/*
 *
 *  Bluetooth HCI UART driver
 *
 *  Copyright (C) 2000-2001  Qualcomm Incorporated
 *  Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
 *  Copyright (C) 2004-2005  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright 2014-2020 NXP
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

#include "include/hci.h"
#include "hci_uart.h"
#include "bt_drv.h"
#include "mbt_char.h"
#include "hci_wrapper.h"

/* HU device list */
LIST_HEAD(hu_dev_list);
static DEFINE_SPINLOCK(hu_list_lock);

#define VERSION "2.2-M2614100"

/** Default Driver mode */
int drv_mode = (DRV_MODE_BT | DRV_MODE_FM | DRV_MODE_NFC);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
int reset = 0;
#else
bool reset = 0;
#endif

static struct hci_uart_proto *hup[HCI_UART_MAX_PROTO];

int
hci_uart_register_proto(struct hci_uart_proto *p)
{
	if (p->id >= HCI_UART_MAX_PROTO)
		return -EINVAL;

	if (hup[p->id])
		return -EEXIST;

	hup[p->id] = p;

	return 0;
}

int
hci_uart_unregister_proto(struct hci_uart_proto *p)
{
	if (p->id >= HCI_UART_MAX_PROTO)
		return -EINVAL;

	if (!hup[p->id])
		return -EINVAL;

	hup[p->id] = NULL;

	return 0;
}

static struct hci_uart_proto *
hci_uart_get_proto(unsigned int id)
{
	if (id >= HCI_UART_MAX_PROTO)
		return NULL;

	return hup[id];
}

static inline void
hci_uart_tx_complete(struct hci_uart *hu, int pkt_type)
{
	struct hci_dev *hdev = hu->hdev;
#ifdef CONFIG_BT_HCIUART_PS
	struct tty_struct *tty = hu->tty;
#endif

	/* Update HCI stat counters */
	switch (pkt_type) {
	case HCI_COMMAND_PKT:
		hdev->stat.cmd_tx++;
		break;

	case HCI_ACLDATA_PKT:
		hdev->stat.acl_tx++;
		break;

	case HCI_SCODATA_PKT:
		hdev->stat.cmd_tx++;
		break;
#ifdef CONFIG_BT_HCIUART_PS
	case MRVL_ENTER_PS_CHAR:
		clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
#ifdef CONFIG_MULTI_CARD_PS
		ps_send_char_complete(hu, pkt_type);
#else
		ps_send_char_complete(pkt_type);
#endif
		break;
	case MRVL_EXIT_PS_CHAR:
#ifdef CONFIG_MULTI_CARD_PS
		ps_send_char_complete(hu, pkt_type);
#else
		ps_send_char_complete(pkt_type);
#endif
		break;
#endif
	}
}

static inline struct sk_buff *
hci_uart_dequeue(struct hci_uart *hu)
{
	struct sk_buff *skb = hu->tx_skb;

	if (!skb)
		skb = hu->proto->dequeue(hu);
	else
		hu->tx_skb = NULL;

	return skb;
}

int
hci_uart_tx_wakeup(struct hci_uart *hu)
{

	if (test_and_set_bit(HCI_UART_SENDING, &hu->tx_state)) {
		set_bit(HCI_UART_TX_WAKEUP, &hu->tx_state);
		return 0;
	}
#ifdef CONFIG_BT_HCIUART_PS
#ifdef CONFIG_MULTI_CARD_PS
	if (ps_wakeup(hu)) {
#else
	if (ps_wakeup()) {
#endif
		clear_bit(HCI_UART_SENDING, &hu->tx_state);
		return 0;
	}
#ifdef CONFIG_MULTI_CARD_PS
	ps_start_timer(hu);
#else
	ps_start_timer();
#endif
#endif

	PRINTM(INFO, "hci_uart_tx_wakeup\n");

	schedule_work(&hu->write_work);

	return 0;
}

static void
hci_uart_write_work(struct work_struct *work)
{
	struct hci_uart *hu = container_of(work, struct hci_uart, write_work);
	struct tty_struct *tty = hu->tty;
	struct hci_dev *hdev = hu->hdev;
	struct sk_buff *skb;

restart:
	clear_bit(HCI_UART_TX_WAKEUP, &hu->tx_state);

	while ((skb = hci_uart_dequeue(hu))) {
		int len;

		DBG_HEXDUMP(DAT_D, "hci_uart TX", skb->data, skb->len);

		set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
		len = TTY_FUNC->write(tty, skb->data, skb->len);
		if (hdev == (struct hci_dev *)skb->dev) {
			hdev->stat.byte_tx += len;
		} else {
			struct m_dev *mdev = NULL;
			mdev = (struct m_dev *)skb->dev;
			mdev->stat.byte_tx += len;
		}

		skb_pull(skb, len);
		if (skb->len) {
			hu->tx_skb = skb;
			break;
		}

		hci_uart_tx_complete(hu, bt_cb(skb)->pkt_type);
		kfree_skb(skb);
	}

	if (test_bit(HCI_UART_TX_WAKEUP, &hu->tx_state))
		goto restart;

	clear_bit(HCI_UART_SENDING, &hu->tx_state);
}

/* ------- Interface to HCI layer ------ */
/* Initialize device */
int
hci_uart_open(struct hci_dev *hdev)
{
#if (defined(CONFIG_MULTI_CARD_PS) && defined(CONFIG_BT_HCIUART_PS))
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
#if defined(BT_AMP) && !defined(MBT_EXT)
	struct hci_uart *hu = (struct hci_uart *)hdev->driver_data;
#else
	struct hci_uart *hu = (struct hci_uart *)hci_get_drvdata(hdev);
#endif
#else
	struct hci_uart *hu = (struct hci_uart *)hdev->driver_data;
#endif
#endif
	PRINTM(CMD, "hci uart %s open %p\n", hdev->name, hdev);
	/* Nothing to do for UART driver */
	set_bit(HCI_RUNNING, &hdev->flags);
#ifdef CONFIG_BT_HCIUART_PS
#ifdef CONFIG_MULTI_CARD_PS
	ps_init(hu);
#else
	ps_init();
#endif
#endif
	return 0;
}

/* Reset device */
int
hci_uart_flush(struct hci_dev *hdev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
#if defined(BT_AMP) && !defined(MBT_EXT)
	struct hci_uart *hu = (struct hci_uart *)hdev->driver_data;
#else
	struct hci_uart *hu = (struct hci_uart *)hci_get_drvdata(hdev);
#endif
#else
	struct hci_uart *hu = (struct hci_uart *)hdev->driver_data;
#endif
	struct tty_struct *tty = hu->tty;

	PRINTM(CMD, "hci uart flush hdev %p tty %p\n", hdev, tty);

	if (hu->tx_skb) {
		kfree_skb(hu->tx_skb);
		hu->tx_skb = NULL;
	}

	/* Flush any pending characters in the driver and discipline. */
	tty_ldisc_flush(tty);
	if (TTY_FUNC && TTY_FUNC->flush_buffer)
		TTY_FUNC->flush_buffer(tty);

	if (test_bit(HCI_UART_PROTO_SET, &hu->flags))
		hu->proto->flush(hu);

	return 0;
}

/* Close device */
int
hci_uart_close(struct hci_dev *hdev)
{
	PRINTM(CMD, "hci uart close hdev %p\n", hdev);

	if (!test_and_clear_bit(HCI_RUNNING, &hdev->flags))
		return 0;

	hci_uart_flush(hdev);
	hdev->flush = NULL;
	return 0;
}

/* Send frames from HCI layer */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#if defined(BT_AMP) && !defined(MBT_EXT)
int
hci_uart_send_frame(struct sk_buff *skb)
#else
int
hci_uart_send_frame(struct hci_dev *hdev, struct sk_buff *skb)
#endif
#else
int
hci_uart_send_frame(struct sk_buff *skb)
#endif
{
	struct tty_struct *tty;
	struct hci_uart *hu;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#if defined(BT_AMP) && !defined(MBT_EXT)
	struct hci_dev *hdev = (struct hci_dev *)skb->dev;
#else
	skb->dev = (void *)hdev;
#endif
#else
	struct hci_dev *hdev = (struct hci_dev *)skb->dev;
#endif

	if (!hdev) {
		PRINTM(ERROR, "Frame for uknown device (hdev=NULL)\n");
		return -ENODEV;
	}
	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return -EBUSY;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
#if defined(BT_AMP) && !defined(MBT_EXT)
	hu = (struct hci_uart *)hdev->driver_data;
#else
	hu = (struct hci_uart *)hci_get_drvdata(hdev);
#endif
#else
	hu = (struct hci_uart *)hdev->driver_data;
#endif
	tty = hu->tty;
	if (!tty) {
		PRINTM(ERROR, "tty is not ready\n");
		return -ENODEV;
	}

	PRINTM(DATA, "%s: Tx type %d len %d\n", hdev->name,
	       bt_cb(skb)->pkt_type, skb->len);

	hu->proto->enqueue(hu, skb);

	hci_uart_tx_wakeup(hu);

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
void
hci_uart_destruct(struct hci_dev *hdev)
{
	if (!hdev)
		return;

	PRINTM(CMD, "hci uart destruct %s\n", hdev->name);
	kfree(hdev->driver_data);
}
#endif

static atomic_t intf_cnt = ATOMIC_INIT(0);
struct class *chardev_class = NULL;
/* ------ LDISC part ------ */
/* hci_uart_tty_open
 *
 *     Called when line discipline changed to HCI_UART.
 *
 * Arguments:
 *     tty    pointer to tty info structure
 * Return Value:
 *     0 if success, otherwise error code
 */
static int
hci_uart_tty_open(struct tty_struct *tty)
{
	struct hci_uart *hu = (void *)tty->disc_data;
	unsigned char id = 0;
	unsigned long flags;
	struct list_head *head = &hu_dev_list, *p;

	if (hu)
		return -EEXIST;

	if (!(hu = kzalloc(sizeof(struct hci_uart), GFP_KERNEL))) {
		PRINTM(ERROR, "Can't allocate control structure\n");
		return -ENFILE;
	}

	spin_lock_irqsave(&hu_list_lock, flags);
	{
		/* Find first available device id */
		list_for_each(p, &hu_dev_list) {
			if (list_entry(p, struct hci_uart, hu_list_head)->id !=
			    id)
				 break;
			head = p;
			id++;
		}
		hu->id = id;
	}
	list_add(&hu->hu_list_head, head);
	spin_unlock_irqrestore(&hu_list_lock, flags);

	tty->disc_data = hu;
	hu->tty = tty;
	tty->receive_room = 65536;

	INIT_WORK(&hu->write_work, hci_uart_write_work);
	hu->chardev_class = chardev_class;
	atomic_inc(&intf_cnt);

	spin_lock_init(&hu->rx_lock);

	/* Flush any pending characters in the driver and line discipline. */

	/* FIXME: why is this needed. Note don't use ldisc_ref here as the open
	   path is before the ldisc is referencable */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
	if (tty->ldisc->ops->flush_buffer)
		tty->ldisc->ops->flush_buffer(tty);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
	if (tty->ldisc.ops->flush_buffer)
		tty->ldisc.ops->flush_buffer(tty);
#else
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
#endif
#endif
	if (TTY_FUNC && TTY_FUNC->flush_buffer)
		TTY_FUNC->flush_buffer(tty);

#ifdef CONFIG_BT_HCIUART_PS
#ifdef CONFIG_MULTI_CARD_PS
	if (0 == ps_init_work(hu))
		ps_init_timer(hu);
#else
	ps_init_timer(tty);
#endif
#endif
	return 0;
}

/* hci_uart_tty_close()
 *
 *    Called when the line discipline is changed to something
 *    else, the tty is closed, or the tty detects a hangup.
 */
static void
hci_uart_tty_close(struct tty_struct *tty)
{
	struct hci_uart *hu = (void *)tty->disc_data;
	unsigned long flags;
	struct list_head *p = NULL;
	struct list_head *n = NULL;

	PRINTM(CMD, "hci uart tty close\n");

	/* Detach from the tty */
	tty->disc_data = NULL;

	if (hu) {
		struct hci_dev *hdev = hu->hdev;

#if defined(CONFIG_BT_HCIUART_PS) && defined(CONFIG_MULTI_CARD_PS)
		ps_cancel_timer(hu);
		ps_proc_remove(hu);
#endif

		if (hdev)
			hci_uart_close(hdev);
		cancel_work_sync(&hu->write_work);

		if (test_and_clear_bit(HCI_UART_PROTO_SET, &hu->flags)) {
			hu->proto->close(hu);

			if (drv_mode & DRV_MODE_BT) {
				hci_unregister_dev(hdev);
				hci_free_dev(hdev);
			}
			if (drv_mode & DRV_MODE_FM) {
				mdev_unregister_dev(&hu->m_dev[FM_SEQ], FM_SEQ);
			}
			if (drv_mode & DRV_MODE_NFC) {
				mdev_unregister_dev(&hu->m_dev[NFC_SEQ],
						    NFC_SEQ);
			}
		}
		atomic_dec(&intf_cnt);
		hu->chardev_class = NULL;
		spin_lock_irqsave(&hu_list_lock, flags);
		list_for_each_safe(p, n, &hu_dev_list) {
			if (hu->id ==
			    list_entry(p, struct hci_uart, hu_list_head)->id) {
				list_del(p);
			}
		}
		spin_unlock_irqrestore(&hu_list_lock, flags);
	}
#if defined(CONFIG_BT_HCIUART_PS) && !defined(CONFIG_MULTI_CARD_PS)
	ps_cancel_timer();
#endif
}

/* hci_uart_tty_wakeup()
 *
 *    Callback for transmit wakeup. Called when low level
 *    device driver can accept more send data.
 *
 * Arguments:        tty    pointer to associated tty instance data
 * Return Value:    None
 */
static void
hci_uart_tty_wakeup(struct tty_struct *tty)
{
	struct hci_uart *hu = (void *)tty->disc_data;

	PRINTM(INFO, "hci_uart_tty_wakeup\n");

	if (!hu)
		return;

	clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);

	if (tty != hu->tty)
		return;
	if (test_bit(HCI_UART_PROTO_SET, &hu->flags))
		hci_uart_tx_wakeup(hu);
}

/* hci_uart_tty_receive()
 *
 *     Called by tty low level driver when receive data is
 *     available.
 *
 * Arguments:  tty          pointer to tty isntance data
 *             data         pointer to received data
 *             flags        pointer to flags for data
 *             count        count of received data in bytes
 *
 * Return Value:    None
 */
static void
hci_uart_tty_receive(struct tty_struct *tty, const u8 * data, char *flags,
		     int count)
{
	struct hci_uart *hu = (void *)tty->disc_data;

	if (!hu || tty != hu->tty)
		return;

	if (!test_bit(HCI_UART_PROTO_SET, &hu->flags))
		return;

#ifdef CONFIG_BT_HCIUART_PS
	DBG_HEXDUMP(DAT_D, "hci uart tty rx", (u8 *) data, count);
#ifdef CONFIG_MULTI_CARD_PS
	ps_start_timer(hu);
#else
	ps_start_timer();
#endif
#endif
	spin_lock(&hu->rx_lock);
	hu->proto->recv(hu, (void *)data, count);
	spin_unlock(&hu->rx_lock);

	if (test_and_clear_bit(TTY_THROTTLED, &tty->flags) &&
	    TTY_FUNC->unthrottle)
		TTY_FUNC->unthrottle(tty);
}

static int
hci_uart_register_dev(struct hci_uart *hu)
{

	struct hci_dev *hdev = NULL;
	int ret = 0;

	PRINTM(CMD, "hci_uart_register_dev\n");

	if (drv_mode & DRV_MODE_BT) {
		hdev = hci_alloc_dev();
		if (!hdev) {
			PRINTM(ERROR, "Can't allocate HCI device\n");
			return -ENOMEM;
		}
		hu->hdev = hdev;

		hdev->open = hci_uart_open;
		hdev->close = hci_uart_close;
		hdev->flush = hci_uart_flush;
		hdev->send = hci_uart_send_frame;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
		hdev->destruct = hci_uart_destruct;
		hdev->owner = THIS_MODULE;
		hdev->driver_data = hu;
#else
#if defined(BT_AMP) && !defined(MBT_EXT)
		hdev->driver_data = hu;
#else
		hci_set_drvdata(hdev, hu);
#endif
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
		SET_HCIDEV_DEV(hdev, hu->tty->dev);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
		hdev->bus = HCI_UART;
#else
		hdev->type = HCI_UART;
#endif /* >= 2.6.34 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
		if (!reset)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
			set_bit(HCI_QUIRK_RESET_ON_CLOSE, &hdev->quirks);
#else
			set_bit(HCI_QUIRK_NO_RESET, &hdev->quirks);
#endif
#else
		if (reset)
			set_bit(HCI_QUIRK_RESET_ON_INIT, &hdev->quirks);
#endif /* >= 2.6.29 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
		hdev->dev_type = DEV_TYPE_BT;
#endif
		ret = hci_register_dev(hdev);
		if (ret < 0) {
			PRINTM(ERROR, "Can't register HCI device\n");
			hci_free_dev(hdev);
			return -ENODEV;
		}
		snprintf(hu->m_dev[BT_SEQ].name, sizeof(hu->m_dev[BT_SEQ].name),
			 hdev->name);
	}

	if (drv_mode & DRV_MODE_FM) {
		hu->m_dev[FM_SEQ].driver_data = hu;
		if ((ret = mdev_register_dev(&hu->m_dev[FM_SEQ], FM_SEQ)))
			goto err_mdev_reg;
	}
	if (drv_mode & DRV_MODE_NFC) {
		hu->m_dev[NFC_SEQ].driver_data = hu;
		if ((ret = mdev_register_dev(&hu->m_dev[NFC_SEQ], NFC_SEQ)))
			goto err_mdev_reg;
	}

#if defined(CONFIG_BT_HCIUART_PS) && defined(CONFIG_MULTI_CARD_PS)
	ps_proc_init(hu);
#endif
	return ret;

err_mdev_reg:
	if (drv_mode & DRV_MODE_BT) {
		hci_unregister_dev(hdev);
		hci_free_dev(hdev);
	}
	if (drv_mode & DRV_MODE_FM) {
		mdev_unregister_dev(&hu->m_dev[FM_SEQ], FM_SEQ);
	}
	if (drv_mode & DRV_MODE_NFC) {
		mdev_unregister_dev(&hu->m_dev[NFC_SEQ], NFC_SEQ);
	}
	return ret;
}

static int
hci_uart_set_proto(struct hci_uart *hu, int id)
{
	struct hci_uart_proto *p;
	int err;
	p = hci_uart_get_proto(id);
	if (!p)
		return -EPROTONOSUPPORT;

	err = p->open(hu);
	if (err)
		return err;

	hu->proto = p;

	err = hci_uart_register_dev(hu);
	if (err) {
		p->close(hu);
		return err;
	}
	return 0;
}

/* hci_uart_tty_ioctl()
 *
 *    Process IOCTL system call for the tty device.
 *
 * Arguments:
 *
 *    tty        pointer to tty instance data
 *    file       pointer to open file object for device
 *    cmd        IOCTL command code
 *    arg        argument for IOCTL call (cmd dependent)
 *
 * Return Value:    Command dependent
 */
static int
hci_uart_tty_ioctl(struct tty_struct *tty, struct file *file,
		   unsigned int cmd, unsigned long arg)
{
	struct hci_uart *hu = (void *)tty->disc_data;
	int err = 0;

	PRINTM(CMD, "hci_uart_tty_ioctl: %d\n", cmd);

	/* Verify the status of the device */
	if (!hu)
		return -EBADF;

	switch (cmd) {
	case HCIUARTSETPROTO:
		if (!test_and_set_bit(HCI_UART_PROTO_SET, &hu->flags)) {
			err = hci_uart_set_proto(hu, arg);
			if (err) {
				clear_bit(HCI_UART_PROTO_SET, &hu->flags);
				return err;
			}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
			tty->port->low_latency = 1;
#else
			tty->low_latency = 1;
#endif
		} else
			return -EBUSY;
		break;

	case HCIUARTGETPROTO:
		if (test_bit(HCI_UART_PROTO_SET, &hu->flags))
			return hu->proto->id;
		return -EUNATCH;

	case HCIUARTGETDEVICE:
		if (test_bit(HCI_UART_PROTO_SET, &hu->flags))
			return hu->hdev->id;
		return -EUNATCH;

	default:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
		err = n_tty_ioctl_helper(tty, file, cmd, arg);
#else
		err = n_tty_ioctl(tty, file, cmd, arg);
#endif
		break;
	};

	return err;
}

/*
 * We don't provide read/write/poll interface for user space.
 */
static ssize_t
hci_uart_tty_read(struct tty_struct *tty, struct file *file,
		  unsigned char __user * buf, size_t nr)
{
	return 0;
}

static ssize_t
hci_uart_tty_write(struct tty_struct *tty, struct file *file,
		   const unsigned char *data, size_t count)
{
	return 0;
}

static unsigned int
hci_uart_tty_poll(struct tty_struct *tty, struct file *filp, poll_table * wait)
{
	return 0;
}

static int __init
hci_uart_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
	static struct tty_ldisc_ops hci_uart_ldisc;
#else
	static struct tty_ldisc hci_uart_ldisc;
#endif
	int err;

	PRINTM(MSG, "HCI UART driver ver %s", VERSION);
	chardev_class = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(chardev_class)) {
		PRINTM(ERROR, "Unable to allocate class\n");
		return PTR_ERR(chardev_class);
	}

	/* Register the tty discipline */

	memset(&hci_uart_ldisc, 0, sizeof(hci_uart_ldisc));
	hci_uart_ldisc.magic = TTY_LDISC_MAGIC;
	hci_uart_ldisc.name = "n_hci";
	hci_uart_ldisc.open = hci_uart_tty_open;
	hci_uart_ldisc.close = hci_uart_tty_close;
	hci_uart_ldisc.read = hci_uart_tty_read;
	hci_uart_ldisc.write = hci_uart_tty_write;
	hci_uart_ldisc.ioctl = hci_uart_tty_ioctl;
	hci_uart_ldisc.poll = hci_uart_tty_poll;
	hci_uart_ldisc.receive_buf = hci_uart_tty_receive;
	hci_uart_ldisc.write_wakeup = hci_uart_tty_wakeup;
	hci_uart_ldisc.owner = THIS_MODULE;

	if ((err = tty_register_ldisc(N_HCI, &hci_uart_ldisc))) {
		PRINTM(ERROR, "HCI line discipline registration failed. (%d)\n",
		       err);
		return err;
	}
#ifdef CONFIG_BT_HCIUART_H4
	h4_init();
#endif
#ifdef CONFIG_BT_HCIUART_BCSP
	bcsp_init();
#endif
#ifdef CONFIG_BT_HCIUART_LL
	ll_init();
#endif
#ifdef CONFIG_BT_HCIUART_PS
	proc_init();
#endif
	return 0;
}

static void __exit
hci_uart_exit(void)
{
	int err;
	/** Destroy char device class */
	class_destroy(chardev_class);

#ifdef CONFIG_BT_HCIUART_H4
	h4_deinit();
#endif
#ifdef CONFIG_BT_HCIUART_BCSP
	bcsp_deinit();
#endif
#ifdef CONFIG_BT_HCIUART_LL
	ll_deinit();
#endif

	/* Release tty registration of line discipline */
	if ((err = tty_unregister_ldisc(N_HCI)))
		PRINTM(ERROR, "Can't unregister HCI line discipline (%d)\n",
		       err);

#ifdef CONFIG_BT_HCIUART_PS
	muart_proc_remove();
#endif
}

module_init(hci_uart_init);
module_exit(hci_uart_exit);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
module_param(reset, int, 0644);
#else
module_param(reset, bool, 0644);
#endif
MODULE_PARM_DESC(reset, "Send HCI reset command on initialization");

MODULE_AUTHOR
	("Maxim Krasnyansky <maxk@qualcomm.com>, Marcel Holtmann <marcel@holtmann.org>");
MODULE_DESCRIPTION("Bluetooth HCI UART driver ver " VERSION " (FP-" FPNUM ")");
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
MODULE_ALIAS_LDISC(N_HCI);
module_param(drv_mode, int, 0);
MODULE_PARM_DESC(drv_mode, "Bit 0: BT/AMP/BLE; Bit 1: FM; Bit 2: NFC");
