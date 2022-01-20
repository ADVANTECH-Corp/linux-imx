/** @file hci_wrapper.c
  *
  * @brief This file contains the char device function calls
  *
  * Copyright 2018-2020 NXP
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>

#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/path.h>
#include <linux/namei.h>
#include <linux/mount.h>

#if defined(BT_AMP) && !defined(MBT_EXT)
#include "include/amp/bluetooth/bluetooth.h"
#include "include/amp/bluetooth/hci_core.h"
#else
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#endif
#include <linux/tty.h>

#include "mbt_char.h"
#include "hci_wrapper.h"
#include "bt_drv.h"
#include "hci_uart.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
extern int reset;
#else
extern bool reset;
#endif

#define AID_SYSTEM        1000	/* system server */

#define AID_BLUETOOTH     1002	/* bluetooth subsystem */

/* HCI device list */
LIST_HEAD(hci_dev_list);
int fmchar_minor = 0;
int nfcchar_minor = 0;

/**
 *  @brief This function queries the wrapper device
 *
 *  @param m_dev   A pointer to m_dev structure
 *  @param arg     arguement
 *
 *  @return 	   BT_STATUS_SUCCESS  or other
 */
void
mdev_query(struct m_dev *m_dev, unsigned long arg)
{
	ENTER();
	if (copy_to_user((void *)arg, &m_dev->type, sizeof(m_dev->type)))
		PRINTM(ERROR, "IOCTL_QUERY_TYPE: Fail copy to user\n");

	LEAVE();
}

/**
 *  @brief This function handles the wrapper_dev ioctl
 *
 *  @param hev     A pointer to wrapper_dev structure
 *  @cmd   	   ioctl cmd
 *  @arg   	   argument
 *  @return 	   -ENOIOCTLCMD
 */
int
mdev_ioctl(struct m_dev *m_dev, unsigned int cmd, unsigned long arg)
{
	ENTER();
	LEAVE();
	return -ENOIOCTLCMD;
}

/**
 *  @brief This function handles wrapper device destruct
 *
 *  @param hdev    A pointer to m_dev structure
 *
 *  @return 	   N/A
 */
void
mdev_destruct(struct m_dev *m_dev)
{
	ENTER();
	LEAVE();
	return;
}

/* Send frames from HCI layer */
int
mdev_send_frame(struct m_dev *m_dev, struct sk_buff *skb)
{
	struct tty_struct *tty;
	struct hci_uart *hu;

	if (!m_dev) {
		PRINTM(ERROR, "Frame for uknown device (m_dev=NULL)\n");
		return -ENODEV;
	}
	if (!test_bit(HCI_RUNNING, &m_dev->flags)) {
		return -EBUSY;
	}

	hu = (struct hci_uart *)m_dev->driver_data;
	if (!hu) {
		return -ENODEV;
	}
	tty = hu->tty;
	if (!tty) {
		PRINTM(ERROR, "tty is not ready\n");
		return -ENODEV;
	}

	PRINTM(DATA, "%s: send frame type %d len %d\n", m_dev->name,
	       bt_cb(skb)->pkt_type, skb->len);
	hu->proto->enqueue(hu, skb);

	hci_uart_tx_wakeup(hu);

	return 0;
}

/**
 *  @brief This function flushes the transmit queue
 *
 *  @param m_dev     A pointer to m_dev structure
 *
 *  @return 	   BT_STATUS_SUCCESS
 */

int
mdev_flush(struct m_dev *m_dev)
{
	struct hci_uart *hu = (struct hci_uart *)m_dev->driver_data;
	struct tty_struct *tty = hu->tty;

	PRINTM(CMD, "mdev_flush: m_dev %p tty %p\n", m_dev, tty);

	return 0;
}

/**
 *  @brief This function closes the wrapper device
 *
 *  @param m_dev   A pointer to m_dev structure
 *
 *  @return 	   BT_STATUS_SUCCESS
 */
int
mdev_close(struct m_dev *m_dev)
{
	ENTER();
	mdev_req_lock(m_dev);
	if (!test_and_clear_bit(HCI_RUNNING, &m_dev->flags)) {
		mdev_req_unlock(m_dev);
		LEAVE();
		return 0;
	}
	mdev_req_unlock(m_dev);

	if (m_dev->flush)
		m_dev->flush(m_dev);
	LEAVE();
	return 0;
}

/* ------- Interface to HCI layer ------ */
/* Initialize device */
int
mdev_open(struct m_dev *m_dev)
{
	PRINTM(CMD, "%s open %p\n", m_dev->name, m_dev);
	/* Nothing to do for UART driver */
	set_bit(HCI_RUNNING, &m_dev->flags);
	return 0;
}

/**
 *  @brief This function initializes the wrapper device
 *
 *  @param m_dev   A pointer to m_dev structure
 *
 *  @return 	   BT_STATUS_SUCCESS  or other
 */
void
init_m_dev(struct m_dev *m_dev)
{
	m_dev->dev_pointer = NULL;
	m_dev->type = HCI_UART;
	m_dev->dev_type = 0;
	m_dev->spec_type = 0;
	skb_queue_head_init(&m_dev->rx_q);
	init_waitqueue_head(&m_dev->req_wait_q);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
	init_MUTEX(&m_dev->req_lock);
#else
	sema_init(&m_dev->req_lock, 1);
#endif
	memset(&m_dev->stat, 0, sizeof(struct hci_dev_stats));
	m_dev->open = mdev_open;
	m_dev->close = mdev_close;
	m_dev->flush = mdev_flush;
	m_dev->send = mdev_send_frame;
	m_dev->destruct = mdev_destruct;
	m_dev->ioctl = mdev_ioctl;
	m_dev->query = mdev_query;
	m_dev->owner = THIS_MODULE;

}

/**
 *  @brief Alloc bt device
 *
 *  @return    pointer to structure mbt_dev or NULL
 */
struct hci_dev *
alloc_hci_dev(void)
{
	struct hci_dev *hdev;
	ENTER();

	hdev = kzalloc(sizeof(struct hci_dev), GFP_KERNEL);
	if (!hdev) {
		LEAVE();
		return NULL;
	}

	LEAVE();
	return hdev;
}

/**
 *  @brief Alloc fm device
 *
 *  @return    pointer to structure fm_dev or NULL
 */
struct fm_dev *
alloc_fm_dev(void)
{
	struct fm_dev *fm_dev;
	ENTER();

	fm_dev = kzalloc(sizeof(struct fm_dev), GFP_KERNEL);
	if (!fm_dev) {
		LEAVE();
		return NULL;
	}

	LEAVE();
	return fm_dev;
}

/**
 *  @brief Alloc nfc device
 *
 *  @return    pointer to structure nfc_dev or NULL
 */
struct nfc_dev *
alloc_nfc_dev(void)
{
	struct nfc_dev *nfc_dev;
	ENTER();

	nfc_dev = kzalloc(sizeof(struct nfc_dev), GFP_KERNEL);
	if (!nfc_dev) {
		LEAVE();
		return NULL;
	}

	LEAVE();
	return nfc_dev;
}

/**
 *  @brief Frees m_dev
 *
 *  @return    N/A
 */
void
free_m_dev(struct m_dev *m_dev)
{
	ENTER();
	if (m_dev->dev_pointer)
		kfree(m_dev->dev_pointer);
	LEAVE();
}

/**
 *  @brief Unregister HCI device
 *  @param hdev   pointer to structure hci_dev
 *  @return    0
 */
int
mdev_unregister_dev(struct m_dev *m_dev, u8 mseq)
{
	struct hci_uart *hu = m_dev->driver_data;
	chardev_cleanup_one(m_dev, hu->chardev_class);
	free_m_dev(m_dev);

	LEAVE();
	return 0;
}

int
mdev_register_dev(struct m_dev *m_dev, u8 mseq)
{
	struct fm_dev *fm_dev = NULL;
	struct nfc_dev *nfc_dev = NULL;
	struct hci_uart *hu = m_dev->driver_data;
	struct class *chardev_class = hu->chardev_class;
	struct char_dev *char_dev = NULL;
	char dev_file[16];

	int ret = 0;
	switch (mseq) {
	case FM_SEQ:
			/** alloc fm_dev */
		fm_dev = alloc_fm_dev();
		if (!fm_dev) {
			PRINTM(ERROR, "Can not allocate fm dev\n");
			return -ENOMEM;
		}

			/** init m_dev */
		init_m_dev(m_dev);
		m_dev->dev_type = FM_TYPE;
		m_dev->spec_type = GENERIC_SPEC;
		m_dev->dev_pointer = (void *)fm_dev;
		fmchar_minor = hu->id;

			/** create char device for FM */
		char_dev = kzalloc(sizeof(struct char_dev), GFP_KERNEL);
		if (!char_dev) {
			return -ENOMEM;
		}
		char_dev->minor = FMCHAR_MINOR_BASE + fmchar_minor;
		char_dev->dev_type = FM_TYPE;
		snprintf(fm_dev->name, sizeof(fm_dev->name), "mfmchar%d",
			 fmchar_minor);
		snprintf(dev_file, sizeof(dev_file), "/dev/mfmchar%d",
			 fmchar_minor);

			/** register char dev */
		register_char_dev(char_dev, chardev_class, MODULE_NAME,
				  fm_dev->name);

			/** chmod for FM char device */
		mbtchar_chmod(dev_file, 0666);

			/** register m_dev to FM char device */
		m_dev->index = char_dev->minor;
		char_dev->m_dev = m_dev;

			/** create proc device */
		snprintf(m_dev->name, sizeof(m_dev->name), fm_dev->name);
//                      bt_proc_init(priv, &(priv->bt_dev.m_dev[FM_SEQ]), FM_SEQ);
		break;

	case NFC_SEQ:
			/** alloc nfc_dev */
		nfc_dev = alloc_nfc_dev();
		if (!nfc_dev) {
			PRINTM(ERROR, "Can not allocate nfc dev\n");
			return -ENOMEM;
		}

			/** init m_dev */
		init_m_dev(m_dev);
		m_dev->dev_type = NFC_TYPE;
		m_dev->spec_type = GENERIC_SPEC;
		m_dev->dev_pointer = (void *)nfc_dev;
		nfcchar_minor = hu->id;

			/** create char device for NFC */
		char_dev = kzalloc(sizeof(struct char_dev), GFP_KERNEL);
		if (!char_dev) {
			return -ENOMEM;
		}
		char_dev->minor = NFCCHAR_MINOR_BASE + nfcchar_minor;
		char_dev->dev_type = NFC_TYPE;
		snprintf(nfc_dev->name, sizeof(nfc_dev->name), "mnfcchar%d",
			 nfcchar_minor);
		snprintf(dev_file, sizeof(dev_file), "/dev/mnfcchar%d",
			 nfcchar_minor);

			/** register char dev */
		register_char_dev(char_dev, chardev_class, MODULE_NAME,
				  nfc_dev->name);

			/** chmod for NFC char device */
		mbtchar_chmod(dev_file, 0666);

			/** register m_dev to NFC char device */
		m_dev->index = char_dev->minor;
		char_dev->m_dev = m_dev;

			/** create proc device */
		snprintf(m_dev->name, sizeof(m_dev->name), nfc_dev->name);
//                      bt_proc_init(priv, &(priv->bt_dev.m_dev[NFC_SEQ]), NFC_SEQ);
		break;
	}
	return ret;
}
