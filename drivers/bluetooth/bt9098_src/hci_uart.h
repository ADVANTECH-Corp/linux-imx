/*
 *
 *  Bluetooth HCI UART driver
 *
 *  Copyright (C) 2000-2001  Qualcomm Incorporated
 *  Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
 *  Copyright (C) 2004-2005  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright 2018-2020 NXP
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
#ifndef _HCI_UART_H_
#define _HCI_UART_H_

#include <linux/version.h>
#include "hci_wrapper.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#define TTY_FUNC tty->ops
#else
#define TTY_FUNC tty->driver
#endif

#ifndef N_HCI
#define N_HCI	15
#endif

/* Ioctls */
#define HCIUARTSETPROTO		_IOW('U', 200, int)
#define HCIUARTGETPROTO		_IOR('U', 201, int)
#define HCIUARTGETDEVICE	_IOR('U', 202, int)

/* UART protocols */
#define HCI_UART_MAX_PROTO	5

#define HCI_UART_H4	0
#define HCI_UART_BCSP	1
#define HCI_UART_3WIRE	2
#define HCI_UART_H4DS	3
#define HCI_UART_LL	4
#define MAX_RADIO_FUNC	3
struct hci_uart;

struct hci_uart_proto {
	unsigned int id;
	int (*open) (struct hci_uart * hu);
	int (*close) (struct hci_uart * hu);
	int (*flush) (struct hci_uart * hu);
	int (*recv) (struct hci_uart * hu, void *data, int len);
	int (*enqueue) (struct hci_uart * hu, struct sk_buff * skb);
	struct sk_buff *(*dequeue) (struct hci_uart * hu);
};

struct hci_uart {
	struct tty_struct *tty;
	struct m_dev m_dev[MAX_RADIO_FUNC];
	struct hci_dev *hdev;
	unsigned long flags;

	struct work_struct write_work;
	struct hci_uart_proto *proto;
	void *priv;

	struct sk_buff *tx_skb;
	unsigned long tx_state;
	spinlock_t rx_lock;
#if defined(CONFIG_BT_HCIUART_PS) && defined(CONFIG_MULTI_CARD_PS)
	struct ps_data *psdata;
	struct proc_dir_entry *proc_bt;
	char proc_name[IFNAMSIZ];
#endif
	struct class *chardev_class;
	struct list_head hu_list_head;
	__u16 id;
};

/* HCI_UART flag bits */
#define HCI_UART_PROTO_SET	0

/* TX states  */
#define HCI_UART_SENDING	1
#define HCI_UART_TX_WAKEUP	2

int hci_uart_register_proto(struct hci_uart_proto *p);
int hci_uart_unregister_proto(struct hci_uart_proto *p);
int hci_uart_tx_wakeup(struct hci_uart *hu);

#ifdef CONFIG_BT_HCIUART_H4
int h4_init(void);
int h4_deinit(void);
#endif

#ifdef CONFIG_BT_HCIUART_BCSP
int bcsp_init(void);
int bcsp_deinit(void);
#endif

#ifdef CONFIG_BT_HCIUART_LL
int ll_init(void);
int ll_deinit(void);
#endif

#ifdef CONFIG_BT_HCIUART_PS
#define MRVL_ENTER_PS_CHAR      'D'
#define MRVL_EXIT_PS_CHAR       'W'
int proc_init(void);
void muart_proc_remove(void);
#ifdef CONFIG_MULTI_CARD_PS
int ps_proc_init(struct hci_uart *hu);
void ps_proc_remove(struct hci_uart *hu);
int ps_init_work(struct hci_uart *hu);
void ps_init_timer(struct hci_uart *hu);
void ps_start_timer(struct hci_uart *hu);
void ps_cancel_timer(struct hci_uart *hu);
int ps_wakeup(struct hci_uart *hu);
void ps_init(struct hci_uart *hu);
void ps_check_event_packet(struct hci_uart *hu, struct sk_buff *skb);
void ps_send_char_complete(struct hci_uart *hu, u8 ch);
#else
void ps_init_timer(struct tty_struct *tty);
void ps_start_timer(void);
void ps_cancel_timer(void);
int ps_wakeup(void);
void ps_init(void);
void ps_check_event_packet(struct sk_buff *skb);
void ps_send_char_complete(u8 ch);
#endif
#endif

int hci_uart_open(struct hci_dev *hdev);
/* Close device */
int hci_uart_close(struct hci_dev *hdev);
/* Reset device */
int hci_uart_flush(struct hci_dev *hdev);
/* Send frames from HCI layer */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#if defined(BT_AMP) && !defined(MBT_EXT)
int hci_uart_send_frame(struct sk_buff *skb);
#else
int hci_uart_send_frame(struct hci_dev *hdev, struct sk_buff *skb);
#endif
#else
int hci_uart_send_frame(struct sk_buff *skb);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
void hci_uart_destruct(struct hci_dev *hdev);
#endif
#endif // _HCI_UART_H_
