/** @file hci_wrapper.h
  * @brief This file contains the char device function calls
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
#ifndef _HCI_WRAPPER_H_
#define _HCI_WRAPPER_H_

/** Define module name */
#define MODULE_NAME  "bt_fm_nfc_urt"

/**  Define Seq num */
#define BT_SEQ      0
#define FM_SEQ      1
#define NFC_SEQ     2

/** Define dev type */
#define BT_TYPE     1
#define BT_AMP_TYPE 2
#define FM_TYPE     3
#define NFC_TYPE    4

/** Define spec type */
#define BLUEZ_SPEC     1
#define IANYWHERE_SPEC 2
#define GENERIC_SPEC   3

/** Define lock/unlock wrapper */
#define mdev_req_lock(d)		down(&d->req_lock)
#define mdev_req_unlock(d)		up(&d->req_lock)

/** Define struct m_dev */
struct m_dev {
	char name[10];
	int index;
	unsigned long flags;
	__u8 type;
	spinlock_t lock;
	struct semaphore req_lock;
	struct sk_buff_head rx_q;
	wait_queue_head_t req_wait_q;
	struct hci_dev_stats stat;
	struct module *owner;
	void *dev_pointer;
	int dev_type;
	int spec_type;
	void *driver_data;

	int (*open) (struct m_dev * m_dev);
	int (*close) (struct m_dev * m_dev);
	int (*flush) (struct m_dev * m_dev);
	int (*send) (struct m_dev * m_dev, struct sk_buff * skb);
	void (*destruct) (struct m_dev * m_dev);
	void (*notify) (struct m_dev * m_dev, unsigned int evt);
	int (*ioctl) (struct m_dev * m_dev, unsigned int cmd,
		      unsigned long arg);
	void (*query) (struct m_dev * mdev, unsigned long arg);

};

/** Define struct mbt_dev */
struct mbt_dev {
	/** maybe could add some private member later */
	char name[10];
	unsigned long flags;
};

/** Define 'fm' interface specific struct fm_dev */
struct fm_dev {
	/** maybe could add some private member later */
	char name[10];
	unsigned long flags;
};

/** Define 'nfc' interface specific struct fm_dev */
struct nfc_dev {
	/** maybe could add some private member later */
	char name[10];
	unsigned long flags;
};

/** This function frees m_dev allocation */
void free_m_dev(struct m_dev *m_dev);

void mdev_query(struct m_dev *m_dev, unsigned long arg);

int mdev_ioctl(struct m_dev *m_dev, unsigned int cmd, unsigned long arg);

void mdev_destruct(struct m_dev *m_dev);

int mdev_flush(struct m_dev *m_dev);

int mdev_close(struct m_dev *m_dev);

int mdev_open(struct m_dev *m_dev);

void init_m_dev(struct m_dev *m_dev);

struct hci_dev *alloc_hci_dev(void);

struct fm_dev *alloc_fm_dev(void);

struct nfc_dev *alloc_nfc_dev(void);

void free_m_dev(struct m_dev *m_dev);

/**
 *  @brief This function receives frames
 *
 *  @param skb     A pointer to struct sk_buff
 *  @return 	   0--success otherwise error code
 */
static inline int
mdev_recv_frame(struct sk_buff *skb)
{
	struct m_dev *m_dev = (struct m_dev *)skb->dev;
	if (!m_dev || (!test_bit(HCI_UP, &m_dev->flags)
		       && !test_bit(HCI_INIT, &m_dev->flags))) {
		kfree_skb(skb);
		return -ENXIO;
	}

	/* Incomming skb */
	bt_cb(skb)->incoming = 1;

	/* Time stamp */
	__net_timestamp(skb);

	/* Queue frame for rx task */
	skb_queue_tail(&m_dev->rx_q, skb);

	/* Wakeup rx thread */
	wake_up_interruptible(&m_dev->req_wait_q);

	return 0;
}

int mdev_register_dev(struct m_dev *m_dev, u8 mseq);

int mdev_unregister_dev(struct m_dev *m_dev, u8 mseq);

#endif /* _HCI_WRAPPER_H_ */
