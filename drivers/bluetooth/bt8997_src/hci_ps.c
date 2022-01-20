/*
 *
 *  Bluetooth HCI UART driver
 *
 *
 *  Copyright 2014-2020 NXP
 *
 *  This software file (the File) is distributed by NXP
 *  under the terms of the GNU General Public License Version 2, June 1991
 *  (the License).  You may use, redistribute and/or modify the File in
 *  accordance with the terms and conditions of the License, a copy of which
 *  is available by writing to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 *  worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 *  THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 *  ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 *  this warranty disclaimer.
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
#include <linux/proc_fs.h>

#ifdef PXA9XX
#if defined(PXA950) || defined(PXA920)
#include <mach/mfp.h>
#include <mach/gpio.h>
#else
#include <asm/arch/mfp-pxa9xx.h>
#include <asm/arch/gpio.h>
#endif
#endif
#include "hci_uart.h"
#include "bt_drv.h"

#define DEFAULT_DEBUG_MASK	(DBG_MSG | DBG_FATAL | DBG_ERROR)
u32 drvdbg = DEFAULT_DEBUG_MASK;
/** proc diretory root */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#define PROC_DIR    NULL
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#define PROC_DIR    &proc_root
#else
#define PROC_DIR    proc_net
#endif

#define DEFAULT_BUF_SIZE 512

/** Default file permission */
#define DEFAULT_FILE_PERM  0644

/** Default time period in mili-second */
#define DEFAULT_TIME_PERIOD 2000

/** wakeup method DTR */
#define WAKEUP_METHOD_DTR       0
/** wakeup method break */
#define WAKEUP_METHOD_BREAK     1
/** wakeup method gpio */
#define WAKEUP_METHOD_GPIO     2
/** wake up method EX break */
#define WAKEUP_METHOD_EXT_BREAK  4
/** wake up method RTS */
#define WAKEUP_METHOD_RTS       5

/** wakeup method invalid */
#define  WAKEUP_METHOD_INVALID  0xff
/** ps mode disable */
#define PS_MODE_DISABLE         0
/** ps mode enable */
#define PS_MODE_ENABLE          1
/** ps cmd exit ps  */
#define PS_CMD_EXIT_PS          1
/** ps cmd enter ps */
#define PS_CMD_ENTER_PS         2
/** ps state awake */
#define PS_STATE_AWAKE                0
/** ps state SLEEP */
#define PS_STATE_SLEEP                1

/** OGF */
#define OGF				0x3F
/** Bluetooth command : Sleep mode */
#define BT_CMD_AUTO_SLEEP_MODE		0x23
/** Bluetooth Power State : Enable */
#define BT_PS_ENABLE			0x02
/** Bluetooth Power State : Disable */
#define BT_PS_DISABLE			0x03
/** Bluetooth command: Wakeup method */
#define BT_CMD_WAKEUP_METHOD    0x53

#define BT_HOST_WAKEUP_METHOD_NONE      0x00
#define BT_HOST_WAKEUP_METHOD_DTR       0x01
#define BT_HOST_WAKEUP_METHOD_BREAK     0x02
#define BT_HOST_WAKEUP_METHOD_GPIO      0x03
#define BT_HOST_WAKEUP_DEFAULT_GPIO     5

#define BT_CTRL_WAKEUP_METHOD_DSR       0x00
#define BT_CTRL_WAKEUP_METHOD_BREAK     0x01
#define BT_CTRL_WAKEUP_METHOD_GPIO      0x02
#define BT_CTRL_WAKEUP_METHOD_EXT_BREAK  0x04
#define BT_CTRL_WAKEUP_METHOD_RTS       0x05

#define BT_CTRL_WAKEUP_DEFAULT_GPIO     4

#define  HCI_OP_AUTO_SLEEP_MODE 0xfc23
#define  HCI_OP_WAKEUP_METHOD   0xfc53
#define SEND_WAKEUP_METHOD_CMD          0x01
#define SEND_AUTO_SLEEP_MODE_CMD        0x02

typedef struct _BT_CMD {
    /** OCF OGF */
	u16 ocf_ogf;
    /** Length */
	u8 length;
    /** Data */
	u8 data[4];
} __attribute__ ((packed)) BT_CMD;

/** Proc directory entry */
static struct proc_dir_entry *proc_bt = NULL;

struct ps_data {
	u32 ps_mode;
	u32 cur_psmode;
	u32 ps_state;
	u32 ps_cmd;
	u32 interval;
	u32 wakeupmode;
	u32 cur_wakeupmode;
	u32 send_cmd;
	struct work_struct work;
	struct tty_struct *tty;
	struct timer_list ps_timer;
	u32 timer_on;
};

#ifndef CONFIG_MULTI_CARD_PS
static struct ps_data g_data;
#endif
int c2h_wakeup_mode = BT_HOST_WAKEUP_METHOD_NONE;
int c2h_wakeup_gpio_pin = BT_HOST_WAKEUP_DEFAULT_GPIO;
int wakeupmode = WAKEUP_METHOD_BREAK;
int ps_mode = PS_MODE_DISABLE;
int h2c_wakeup_gpio_pin = BT_CTRL_WAKEUP_DEFAULT_GPIO;
struct proc_data {
    /** Read length */
	int rdlen;
    /** Read buffer */
	char *rdbuf;
    /** Write length */
	int wrlen;
    /** Maximum write length */
	int maxwrlen;
    /** Write buffer */
	char *wrbuf;
};

/** Debug dump buffer length */
#define DBG_DUMP_BUF_LEN 	64
/** Maximum number of dump per line */
#define MAX_DUMP_PER_LINE	16
/** Maximum data dump length */
#define MAX_DATA_DUMP_LEN	48

/** convert string to number */
int
string_to_number(char *s)
{
	int r = 0;
	int base = 0;
	int pn = 1;

	if (strncmp(s, "-", 1) == 0) {
		pn = -1;
		s++;
	}
	if ((strncmp(s, "0x", 2) == 0) || (strncmp(s, "0X", 2) == 0)) {
		base = 16;
		s += 2;
	} else
		base = 10;

	for (s = s; *s != 0; s++) {
		if ((*s >= '0') && (*s <= '9'))
			r = (r * base) + (*s - '0');
		else if ((*s >= 'A') && (*s <= 'F'))
			r = (r * base) + (*s - 'A' + 10);
		else if ((*s >= 'a') && (*s <= 'f'))
			r = (r * base) + (*s - 'a' + 10);
		else
			break;
	}

	return (r * pn);
}

static int
is_device_ready(struct hci_uart *hu)
{
	struct hci_dev *hdev = NULL;
	if (!hu) {
		PRINTM(ERROR, "hu is NULL\n");
		return -ENODEV;
	}
	if (!hu->proto || !hu->hdev || !hu->tty) {
		PRINTM(ERROR, "Device not ready! proto=%p, hdev=%p, tty=%p\n",
		       hu->proto, hu->hdev, hu->tty);
		return -ENODEV;
	}
	hdev = hu->hdev;
	if (!test_bit(HCI_RUNNING, &hdev->flags)) {
		PRINTM(ERROR, "HCI_RUNNING is not set\n");
		return -EBUSY;
	}
	return 0;
}

/*
 * Builds and sends an PS command packet.
 */
static int
send_ps_cmd(u8 cmd, struct hci_uart *hu)
{
	int err = 0;
	struct sk_buff *skb = NULL;
	BT_CMD *pCmd;

	PRINTM(CMD, "hu %p cmd 0x%x\n", hu, cmd);

	/* allocate packet */
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (!skb) {
		PRINTM(ERROR, "cannot allocate memory for HCILL packet\n");
		err = -ENOMEM;
		goto out;
	}

	pCmd = (BT_CMD *) skb->data;
	pCmd->ocf_ogf = (OGF << 10) | BT_CMD_AUTO_SLEEP_MODE;
	pCmd->length = 1;
	if (cmd == PS_MODE_ENABLE)
		pCmd->data[0] = BT_PS_ENABLE;
	else
		pCmd->data[0] = BT_PS_DISABLE;

	bt_cb(skb)->pkt_type = HCI_COMMAND_PKT;
	skb_put(skb, sizeof(BT_CMD) - 4 + pCmd->length);
	skb->dev = (void *)hu->hdev;

	/* send packet */
	hu->proto->enqueue(hu, skb);
	hci_uart_tx_wakeup(hu);

out:
	return err;
}

/*
 * Builds and sends an wake up method command packet.
 */
static int
send_wakeup_method_cmd(u8 cmd, struct hci_uart *hu)
{
	int err = 0;
	struct sk_buff *skb = NULL;
	BT_CMD *pCmd;

	PRINTM(CMD, "hu %p cmd 0x%x\n", hu, cmd);

	/* allocate packet */
	skb = bt_skb_alloc(sizeof(BT_CMD), GFP_ATOMIC);
	if (!skb) {
		PRINTM(ERROR, "cannot allocate memory for HCILL packet\n");
		err = -ENOMEM;
		goto out;
	}

	pCmd = (BT_CMD *) skb->data;
	pCmd->ocf_ogf = (OGF << 10) | BT_CMD_WAKEUP_METHOD;
	pCmd->length = 4;
	pCmd->data[0] = c2h_wakeup_mode;
	pCmd->data[1] = c2h_wakeup_gpio_pin;
	switch (cmd) {
	case WAKEUP_METHOD_DTR:
		pCmd->data[2] = BT_CTRL_WAKEUP_METHOD_DSR;
		break;
	case WAKEUP_METHOD_EXT_BREAK:
		pCmd->data[2] = BT_CTRL_WAKEUP_METHOD_EXT_BREAK;
		break;
	case WAKEUP_METHOD_RTS:
		pCmd->data[2] = BT_CTRL_WAKEUP_METHOD_RTS;
		break;
	case WAKEUP_METHOD_GPIO:
		pCmd->data[2] = BT_CTRL_WAKEUP_METHOD_GPIO;
		break;
	case WAKEUP_METHOD_BREAK:
	default:
		pCmd->data[2] = BT_CTRL_WAKEUP_METHOD_BREAK;
		break;
	}
	pCmd->data[3] = h2c_wakeup_gpio_pin;

	bt_cb(skb)->pkt_type = HCI_COMMAND_PKT;

	skb_put(skb, sizeof(BT_CMD) - 4 + pCmd->length);
	skb->dev = (void *)hu->hdev;

	/* send packet */
	hu->proto->enqueue(hu, skb);
	hci_uart_tx_wakeup(hu);

out:
	return err;
}

/*
 * Builds and sends an char packet.
 */
static int
send_char(char ch, struct hci_uart *hu)
{
	int err = 0;
	struct sk_buff *skb = NULL;

	PRINTM(INFO, "hu %p char=%c 0x%x\n", hu, ch, ch);

	/* allocate packet */
	skb = bt_skb_alloc(1, GFP_ATOMIC);
	if (!skb) {
		PRINTM(ERROR, "cannot allocate memory for HCILL packet\n");
		err = -ENOMEM;
		goto out;
	}
	bt_cb(skb)->pkt_type = ch;
	skb->dev = (void *)hu->hdev;

	/* send packet */
	if (hu->tx_skb)
		hu->proto->enqueue(hu, skb);
	else {
		memcpy(skb_push(skb, 1), &bt_cb(skb)->pkt_type, 1);
		hu->tx_skb = skb;
	}
	hci_uart_tx_wakeup(hu);

out:
	return err;
}

/** This function handle the generic file close */
static void
proc_on_close(struct inode *inode, struct file *file)
{
	struct proc_data *pdata = file->private_data;
	char *line;
#ifdef CONFIG_MULTI_CARD_PS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
	struct hci_uart *hu = PDE_DATA(inode);
#else
	struct hci_uart *hu = PDE(inode)->data;
#endif
	struct ps_data *psdata = hu->psdata;
	u32 ps_mode = psdata->ps_mode;
	u32 wakeup = psdata->cur_wakeupmode;
#else
	struct hci_uart *hu = NULL;
	u32 ps_mode = g_data.ps_mode;
	u32 wakeup = g_data.cur_wakeupmode;
#endif
	if (!pdata->wrlen)
		return;
	line = pdata->wrbuf;
	while (line[0]) {
		if (!strncmp(line, "psmode", strlen("psmode"))) {
			line += strlen("psmode") + 1;
			ps_mode = (u32) string_to_number(line);
			if (ps_mode > PS_MODE_ENABLE)
#ifdef CONFIG_MULTI_CARD_PS
				ps_mode = psdata->ps_mode;
#else
				ps_mode = g_data.ps_mode;
#endif
		}
		if (!strncmp(line, "interval", strlen("interval"))) {
			line += strlen("interval") + 1;
#ifdef CONFIG_MULTI_CARD_PS
			psdata->interval = (u32) string_to_number(line);
#else
			g_data.interval = (u32) string_to_number(line);
#endif
		}
		if (!strncmp(line, "drvdbg", strlen("drvdbg"))) {
			line += strlen("drvdbg") + 1;
			drvdbg = (u32) string_to_number(line);
		}
		if (!strncmp(line, "wakeupmode", strlen("wakeupmode"))) {
			line += strlen("wakeupmode") + 1;
			wakeup = (u32) string_to_number(line);
			if (wakeup > WAKEUP_METHOD_RTS)
#ifdef CONFIG_MULTI_CARD_PS
				wakeup = psdata->cur_wakeupmode;
#else
				wakeup = g_data.cur_wakeupmode;
#endif
		}
		while (line[0] && line[0] != '\n')
			line++;
		if (line[0])
			line++;
	}
#ifdef CONFIG_MULTI_CARD_PS
	if ((psdata->cur_psmode == PS_MODE_DISABLE) &&
	    (ps_mode == PS_MODE_DISABLE) &&
	    (wakeup != psdata->cur_wakeupmode)) {
		psdata->wakeupmode = wakeup;
		if (psdata->tty) {
			if (0 == is_device_ready(hu)) {
				psdata->send_cmd |= SEND_WAKEUP_METHOD_CMD;
				send_wakeup_method_cmd(psdata->wakeupmode, hu);
			}
		}
	}
	if (ps_mode != psdata->ps_mode)
		psdata->ps_mode = ps_mode;
	if (ps_mode != psdata->cur_psmode) {
		if (psdata->tty) {
			if (0 == is_device_ready(hu)) {
				psdata->send_cmd |= SEND_AUTO_SLEEP_MODE_CMD;
				send_ps_cmd(psdata->ps_mode, hu);
			}
		}
	}
#else
	if ((g_data.cur_psmode == PS_MODE_DISABLE) &&
	    (ps_mode == PS_MODE_DISABLE) && (wakeup != g_data.cur_wakeupmode)) {
		g_data.wakeupmode = wakeup;
		if (g_data.tty) {
			hu = (void *)g_data.tty->disc_data;
			if (0 == is_device_ready(hu)) {
				g_data.send_cmd |= SEND_WAKEUP_METHOD_CMD;
				send_wakeup_method_cmd(g_data.wakeupmode, hu);
			}
		}
	}
	if (ps_mode != g_data.ps_mode)
		g_data.ps_mode = ps_mode;
	if (ps_mode != g_data.cur_psmode) {
		if (g_data.tty) {
			hu = (void *)g_data.tty->disc_data;
			if (0 == is_device_ready(hu)) {
				g_data.send_cmd |= SEND_AUTO_SLEEP_MODE_CMD;
				send_ps_cmd(g_data.ps_mode, hu);
			}
		}
	}
#endif
	return;
}

/** This function handle generic proc file close */
static int
proc_close(struct inode *inode, struct file *file)
{
	struct proc_data *pdata = file->private_data;
	if (pdata) {
		proc_on_close(inode, file);
		if (pdata->rdbuf)
			kfree(pdata->rdbuf);
		if (pdata->wrbuf)
			kfree(pdata->wrbuf);
		kfree(pdata);
	}
	return 0;
}

/** This function handle generic proc file read */
static ssize_t
proc_read(struct file *file, char __user * buffer, size_t len, loff_t * offset)
{
	loff_t pos = *offset;
	struct proc_data *pdata = (struct proc_data *)file->private_data;
	if ((!pdata->rdbuf) || (pos < 0))
		return -EINVAL;
	if (pos >= pdata->rdlen)
		return 0;
	if (len > pdata->rdlen - pos)
		len = pdata->rdlen - pos;
	if (copy_to_user(buffer, pdata->rdbuf + pos, len))
		return -EFAULT;
	*offset = pos + len;
	return len;
}

/** This function handle generic proc file write */
static ssize_t
proc_write(struct file *file,
	   const char __user * buffer, size_t len, loff_t * offset)
{
	loff_t pos = *offset;
	struct proc_data *pdata = (struct proc_data *)file->private_data;

	if (!pdata->wrbuf || (pos < 0))
		return -EINVAL;
	if (pos >= pdata->maxwrlen)
		return 0;
	if (len > pdata->maxwrlen - pos)
		len = pdata->maxwrlen - pos;
	if (copy_from_user(pdata->wrbuf + pos, buffer, len))
		return -EFAULT;
	if (pos + len > pdata->wrlen)
		pdata->wrlen = len + file->f_pos;
	*offset = pos + len;
	return len;
}

/** This function handle the generic file open */
static int
proc_open(struct inode *inode, struct file *file)
{
	struct proc_data *pdata;
	char *p;
#ifdef CONFIG_MULTI_CARD_PS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
	struct hci_uart *hu = PDE_DATA(inode);
#else
	struct hci_uart *hu = PDE(inode)->data;
#endif
	struct ps_data *psdata = hu->psdata;
#endif
	if ((file->private_data =
	     kzalloc(sizeof(struct proc_data), GFP_KERNEL)) == NULL) {
		PRINTM(ERROR, "Can not allocate memmory for proc_data\n");
		return -ENOMEM;
	}
	pdata = (struct proc_data *)file->private_data;
	if ((pdata->rdbuf = kmalloc(DEFAULT_BUF_SIZE, GFP_KERNEL)) == NULL) {
		PRINTM(ERROR, "Can not allocate memory for rdbuf\n");
		kfree(file->private_data);
		return -ENOMEM;
	}
	if ((pdata->wrbuf = kzalloc(DEFAULT_BUF_SIZE, GFP_KERNEL)) == NULL) {
		PRINTM(ERROR, "Can not allocate memory for wrbuf\n");
		kfree(pdata->rdbuf);
		kfree(file->private_data);
		return -ENOMEM;
	}
	pdata->maxwrlen = DEFAULT_BUF_SIZE;
	p = pdata->rdbuf;
#ifdef CONFIG_MULTI_CARD_PS
	p += sprintf(p, "psmode=%d\n", psdata->ps_mode);
	p += sprintf(p, "psstate=%d\n", psdata->ps_state);
	p += sprintf(p, "interval=%d\n", psdata->interval);
	p += sprintf(p, "wakeupmode=%d\n", psdata->wakeupmode);
	p += sprintf(p, "current psmode=%d\n", psdata->cur_psmode);
	p += sprintf(p, "current wakeupmode=%d\n", psdata->cur_wakeupmode);
	p += sprintf(p, "sendcmd=%d\n", psdata->send_cmd);
#else
	p += sprintf(p, "psmode=%d\n", g_data.ps_mode);
	p += sprintf(p, "psstate=%d\n", g_data.ps_state);
	p += sprintf(p, "interval=%d\n", g_data.interval);
	p += sprintf(p, "wakeupmode=%d\n", g_data.wakeupmode);
	p += sprintf(p, "current psmode=%d\n", g_data.cur_psmode);
	p += sprintf(p, "current wakeupmode=%d\n", g_data.cur_wakeupmode);
	p += sprintf(p, "sendcmd=%d\n", g_data.send_cmd);
#endif
	p += sprintf(p, "drvdbg=%d\n", drvdbg);
	pdata->rdlen = strlen(pdata->rdbuf);
	return 0;
}

static struct file_operations proc_rw_ops = {
	.read = proc_read,
	.write = proc_write,
	.open = proc_open,
	.release = proc_close
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
void
ps_timeout_func(struct timer_list *t)
{
	struct ps_data *data = from_timer(data, t, ps_timer);
#else
void
ps_timeout_func(unsigned long context)
{
	struct ps_data *data = (struct ps_data *)context;
#endif
	struct tty_struct *tty = data->tty;
	struct hci_uart *hu = NULL;
	data->timer_on = 0;
	if (!data->tty)
		return;
	hu = (struct hci_uart *)tty->disc_data;
	if (!hu)
		return;
	if (test_bit(HCI_UART_SENDING, &hu->tx_state)) {
#ifdef CONFIG_MULTI_CARD_PS
		ps_start_timer(hu);
#else
		ps_start_timer();
#endif
	} else {
		data->ps_cmd = PS_CMD_ENTER_PS;
		schedule_work(&data->work);
	}
}

static void
set_dtr(struct tty_struct *tty, int on_off)
{
#ifdef PXA9XX
	if (on_off) {
		gpio_set_value(mfp_to_gpio(MFP_PIN_GPIO13), 0);
		PRINTM(CMD, "Set DTR ON\n");
	} else {
		gpio_set_value(mfp_to_gpio(MFP_PIN_GPIO13), 1);
		PRINTM(CMD, "Clear DTR\n");
	}
#else
	u32 old_state = 0;
	u32 new_state = 0;
	if (TTY_FUNC->tiocmget) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
		old_state = TTY_FUNC->tiocmget(tty, NULL);
#else
		old_state = TTY_FUNC->tiocmget(tty);
#endif
		if (on_off)
			new_state = old_state | TIOCM_DTR;
		else
			new_state = old_state & ~TIOCM_DTR;
		if (new_state == old_state)
			return;
		if (TTY_FUNC->tiocmset) {
			if (on_off) {
				PRINTM(CMD, "Set DTR ON\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
				TTY_FUNC->tiocmset(tty, NULL, TIOCM_DTR, 0);
#else
				TTY_FUNC->tiocmset(tty, TIOCM_DTR, 0);
#endif
			} else {
				PRINTM(CMD, "Clear DTR\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
				TTY_FUNC->tiocmset(tty, NULL, 0, TIOCM_DTR);
#else
				TTY_FUNC->tiocmset(tty, 0, TIOCM_DTR);
#endif
			}
		}
	}
#endif
	return;
}

static void
set_break(struct tty_struct *tty, int on_off)
{
	if (TTY_FUNC->break_ctl) {
		if (on_off) {
			PRINTM(CMD, "Turn on break\n");
			TTY_FUNC->break_ctl(tty, -1);	/* turn on break */
		} else {
			PRINTM(CMD, "Turn off break\n");
			TTY_FUNC->break_ctl(tty, 0);	/* turn off break */
		}
	}
	return;
}

static int
get_cts(struct tty_struct *tty)
{
	u32 state = 0;
	if (TTY_FUNC->tiocmget) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
		state = TTY_FUNC->tiocmget(tty, NULL);
#else
		state = TTY_FUNC->tiocmget(tty);
#endif
		if (state & TIOCM_CTS) {
			PRINTM(CMD, "CTS is low\n");
			return 1;	// CTS LOW
		} else {
			PRINTM(CMD, "CTS is high\n");
			return 0;	// CTS HIGH
		}
	}
	return -1;
}

static void
set_rts(struct tty_struct *tty, int on_off)
{
	u32 old_state = 0;
	u32 new_state = 0;
	if (TTY_FUNC->tiocmget) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
		old_state = TTY_FUNC->tiocmget(tty, NULL);
#else
		old_state = TTY_FUNC->tiocmget(tty);
#endif
		if (on_off)
			new_state = old_state | TIOCM_RTS;
		else
			new_state = old_state & ~TIOCM_RTS;
		if (new_state == old_state)
			return;
		if (TTY_FUNC->tiocmset) {
			if (on_off) {
				PRINTM(CMD, "Set RTS ON\n");	// set RTS high
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
				TTY_FUNC->tiocmset(tty, NULL, TIOCM_RTS, 0);
#else
				TTY_FUNC->tiocmset(tty, TIOCM_RTS, 0);
#endif
			} else {
				PRINTM(CMD, "Clear RTS\n");	// set RTS LOW
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
				TTY_FUNC->tiocmset(tty, NULL, 0, TIOCM_RTS);
#else
				TTY_FUNC->tiocmset(tty, 0, TIOCM_RTS);
#endif
			}
		}
	}
	return;
}

static void
ps_control(struct ps_data *data, u8 ps_state)
{
	struct hci_uart *hu = NULL;
	if (data->ps_state == ps_state)
		return;
	if (data->tty) {
		switch (data->cur_wakeupmode) {
		case WAKEUP_METHOD_DTR:
			if (ps_state == PS_STATE_AWAKE)
				set_dtr(data->tty, 1);	// DTR ON
			else
				set_dtr(data->tty, 0);	// DTR OFF
			data->ps_state = ps_state;
			break;
		case WAKEUP_METHOD_BREAK:
			if (ps_state == PS_STATE_AWAKE)
				set_break(data->tty, 0);	// break OFF
			else
				set_break(data->tty, 1);	// break ON
			data->ps_state = ps_state;
			break;
		case WAKEUP_METHOD_EXT_BREAK:
			if (ps_state == PS_STATE_AWAKE) {
				set_break(data->tty, 1);	// break ON
				set_break(data->tty, 0);	// break OFF
				data->ps_state = ps_state;
			} else {
				hu = (struct hci_uart *)data->tty->disc_data;
				if (0 == is_device_ready(hu))
					send_char(MRVL_ENTER_PS_CHAR, hu);
			}
			break;
		case WAKEUP_METHOD_RTS:
			if (ps_state == PS_STATE_AWAKE) {
				set_rts(data->tty, 0);	// RTS to high
				mdelay(5);
				set_rts(data->tty, 1);	// RTS to low
				data->ps_state = ps_state;
				hu = (struct hci_uart *)data->tty->disc_data;
				if (0 == is_device_ready(hu))
					send_char(MRVL_EXIT_PS_CHAR, hu);
			} else {
				hu = (struct hci_uart *)data->tty->disc_data;
				if (0 == is_device_ready(hu))
					send_char(MRVL_ENTER_PS_CHAR, hu);
			}
			break;
		case WAKEUP_METHOD_GPIO:
			/* todo: implement gpio wakeup method */
			break;
		default:
			break;
		}
		if (ps_state == PS_STATE_AWAKE) {
			hu = (struct hci_uart *)data->tty->disc_data;
			/* actually send the packets */
			PRINTM(DATA, "Send tx data...\n");
			if (hu)
				hci_uart_tx_wakeup(hu);
		}
	}
}

static void
ps_work_func(struct work_struct *work)
{
	struct ps_data *data = container_of(work, struct ps_data, work);
	if (data->tty) {
		if ((data->ps_cmd == PS_CMD_ENTER_PS) &&
		    (data->cur_psmode == PS_MODE_ENABLE)) {
			ps_control(data, PS_STATE_SLEEP);
		} else if (data->ps_cmd == PS_CMD_EXIT_PS) {
			ps_control(data, PS_STATE_AWAKE);
		}
	}
}

#ifdef CONFIG_MULTI_CARD_PS
int
ps_init_work(struct hci_uart *hu)
{
	struct ps_data *psdata;

	if (!(psdata = kzalloc(sizeof(struct ps_data), GFP_KERNEL))) {
		PRINTM(ERROR, "Can't allocate control structure\n");
		return -ENFILE;
	}
	PRINTM(MSG, "ps_init_work...\n");
	hu->psdata = psdata;

	memset(psdata, 0, sizeof(*psdata));
	psdata->interval = DEFAULT_TIME_PERIOD;
	psdata->timer_on = 0;
	psdata->tty = NULL;
	psdata->ps_state = PS_STATE_AWAKE;
	psdata->ps_mode = ps_mode;
	psdata->ps_cmd = 0;
	psdata->send_cmd = 0;
	switch (wakeupmode) {
	case WAKEUP_METHOD_DTR:
		psdata->wakeupmode = WAKEUP_METHOD_DTR;
		break;
	case WAKEUP_METHOD_EXT_BREAK:
		psdata->wakeupmode = WAKEUP_METHOD_EXT_BREAK;
		break;
	case WAKEUP_METHOD_RTS:
		psdata->wakeupmode = WAKEUP_METHOD_RTS;
		break;
	case WAKEUP_METHOD_GPIO:
		psdata->wakeupmode = WAKEUP_METHOD_GPIO;
		break;
	case WAKEUP_METHOD_BREAK:
	default:
		psdata->wakeupmode = WAKEUP_METHOD_BREAK;
		break;
	}
	psdata->cur_psmode = PS_MODE_DISABLE;
	psdata->cur_wakeupmode = WAKEUP_METHOD_INVALID;
	INIT_WORK(&psdata->work, ps_work_func);
	return 0;
}
#else /* CONFIG_MULTI_CARD_PS */
void
ps_init_work(void)
{
	PRINTM(MSG, "ps_init_work...\n");
	memset(&g_data, 0, sizeof(g_data));
	g_data.interval = DEFAULT_TIME_PERIOD;
	g_data.timer_on = 0;
	g_data.tty = NULL;
	g_data.ps_state = PS_STATE_AWAKE;
	g_data.ps_mode = ps_mode;
	g_data.ps_cmd = 0;
	g_data.send_cmd = 0;
	switch (wakeupmode) {
	case WAKEUP_METHOD_DTR:
		g_data.wakeupmode = WAKEUP_METHOD_DTR;
		break;
	case WAKEUP_METHOD_EXT_BREAK:
		g_data.wakeupmode = WAKEUP_METHOD_EXT_BREAK;
		break;
	case WAKEUP_METHOD_RTS:
		g_data.wakeupmode = WAKEUP_METHOD_RTS;
		break;
	case WAKEUP_METHOD_GPIO:
		g_data.wakeupmode = WAKEUP_METHOD_GPIO;
		break;
	case WAKEUP_METHOD_BREAK:
	default:
		g_data.wakeupmode = WAKEUP_METHOD_BREAK;
		break;
	}
	g_data.cur_psmode = PS_MODE_DISABLE;
	g_data.cur_wakeupmode = WAKEUP_METHOD_INVALID;
	INIT_WORK(&g_data.work, ps_work_func);
}
#endif /* CONFIG_MULTI_CARD_PS */

/** This function init proc entry  */
int
proc_init(void)
{
#ifndef CONFIG_MULTI_CARD_PS
	u8 ret = 0;
	struct proc_dir_entry *entry;
	if (!proc_bt) {
		proc_bt = proc_mkdir("mbt_uart", PROC_DIR);
		if (!proc_bt) {
			PRINTM(ERROR, "Could not mkdir mbt_uart!\n");
			ret = -1;
			goto done;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
		entry = proc_create_data("config", S_IFREG | DEFAULT_FILE_PERM,
					 proc_bt, &proc_rw_ops, NULL);
		if (entry == NULL)
#else
		entry = create_proc_entry("config", S_IFREG | DEFAULT_FILE_PERM,
					  proc_bt);
		if (entry) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
			entry->owner = THIS_MODULE;
#endif
			entry->proc_fops = &proc_rw_ops;
		} else
#endif
			PRINTM(ERROR, "MUART: Fail to create proc\n");
	}
	ps_init_work();
done:
	return ret;
#else /* CONFIG_MULTI_CARD_PS */
	if (!proc_bt) {
		proc_bt = proc_mkdir("mbt_uart", PROC_DIR);
		if (!proc_bt) {
			PRINTM(ERROR, "Could not mkdir mbt_uart!\n");
			return -1;
		}
	}
	return 0;
#endif /* CONFIG_MULTI_CARD_PS */
}

/** remove proc file */
void
muart_proc_remove(void)
{
	if (proc_bt) {
#ifndef CONFIG_MULTI_CARD_PS
		remove_proc_entry("config", proc_bt);
#endif
		remove_proc_entry("mbt_uart", PROC_DIR);
		proc_bt = NULL;
	}
	return;
}

#ifdef CONFIG_MULTI_CARD_PS
void
ps_send_char_complete(struct hci_uart *hu, u8 ch)
{
	struct ps_data *psdata = hu->psdata;

	PRINTM(CMD, "Send char %c done\n", ch);
	if (psdata->ps_mode == PS_MODE_ENABLE) {
		if (ch == MRVL_ENTER_PS_CHAR)
			psdata->ps_state = PS_STATE_SLEEP;
		else if (ch == MRVL_EXIT_PS_CHAR)
			psdata->ps_state = PS_STATE_AWAKE;
	}
}

void
ps_init_timer(struct hci_uart *hu)
{
	struct ps_data *psdata = hu->psdata;
	PRINTM(MSG, "ps_init_timer...\n");
	psdata->timer_on = 0;
	psdata->tty = hu->tty;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
	timer_setup(&psdata->ps_timer, ps_timeout_func, 0);
#else
	init_timer(&psdata->ps_timer);
	psdata->ps_timer.function = ps_timeout_func;
	psdata->ps_timer.data = (unsigned long)psdata;
#endif
	return;
}

void
ps_start_timer(struct hci_uart *hu)
{
	struct ps_data *psdata = hu->psdata;

	if (psdata->cur_psmode == PS_MODE_ENABLE) {
		psdata->timer_on = 1;
		mod_timer(&psdata->ps_timer,
			  jiffies + (psdata->interval * HZ) / 1000);
	}
}

void
ps_cancel_timer(struct hci_uart *hu)
{
	struct ps_data *psdata = hu->psdata;

	if (psdata) {
		flush_scheduled_work();
		if (psdata->timer_on)
			del_timer(&psdata->ps_timer);
		if ((psdata->cur_psmode == PS_MODE_ENABLE) &&
		    (psdata->cur_wakeupmode == WAKEUP_METHOD_BREAK)) {
			// set_break off
			set_break(psdata->tty, 0);
		}
		psdata->tty = NULL;
		kfree(psdata);
	}
	return;
}

int
ps_wakeup(struct hci_uart *hu)
{
	struct ps_data *psdata = hu->psdata;

	if (psdata->ps_state == PS_STATE_AWAKE)
		return 0;
	psdata->ps_cmd = PS_CMD_EXIT_PS;
	schedule_work(&psdata->work);
	return 1;
}

void
ps_init(struct hci_uart *hu)
{
	struct ps_data *psdata = hu->psdata;
	int mode = 0;
	struct ktermios old_termios;
	PRINTM(MSG, "ps_init...\n");
	if (!psdata || !psdata->tty)
		return;
	if (1 != get_cts(psdata->tty)) {
		/* firmware is sleeping */
		mode = psdata->cur_wakeupmode;
		if (mode == WAKEUP_METHOD_INVALID)
			mode = wakeupmode;
		switch (mode) {
		case WAKEUP_METHOD_BREAK:
			// set RTS
			set_rts(psdata->tty, 1);
			// break on
			set_break(psdata->tty, 1);
			// break off
			set_break(psdata->tty, 0);
			mdelay(5);
			break;
		case WAKEUP_METHOD_DTR:
			// set RTS
			set_rts(psdata->tty, 1);
			set_dtr(psdata->tty, 0);
			set_dtr(psdata->tty, 1);
			mdelay(5);
			break;
		case WAKEUP_METHOD_GPIO:
			/* todo: implement gpio wakeup method */
			break;
		default:
			break;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
		old_termios = psdata->tty->termios;
		psdata->tty->termios.c_cflag &= ~CRTSCTS;	/* Clear the
								   flow control
								 */
		psdata->TTY_FUNC->set_termios(psdata->tty, &old_termios);
		old_termios = psdata->tty->termios;
		psdata->tty->termios.c_cflag |= CRTSCTS;	/* Enable the
								   flow control
								 */
		psdata->TTY_FUNC->set_termios(psdata->tty, &old_termios);
#else
		old_termios = *(psdata->tty->termios);
		psdata->tty->termios->c_cflag &= ~CRTSCTS;	/* Clear the
								   flow control
								 */
		psdata->TTY_FUNC->set_termios(psdata->tty, &old_termios);
		old_termios = *(psdata->tty->termios);
		psdata->tty->termios->c_cflag |= CRTSCTS;	/* Enable the
								   flow control
								 */
		psdata->TTY_FUNC->set_termios(psdata->tty, &old_termios);
#endif
	}

	psdata->send_cmd = 0;
	if (0 == is_device_ready(hu)) {

		if (psdata->cur_wakeupmode != psdata->wakeupmode) {
			psdata->send_cmd |= SEND_WAKEUP_METHOD_CMD;
			send_wakeup_method_cmd(psdata->wakeupmode, hu);
		}
		if (psdata->cur_psmode != psdata->ps_mode) {
			psdata->send_cmd |= SEND_AUTO_SLEEP_MODE_CMD;
			send_ps_cmd(psdata->ps_mode, hu);
		}
	}
}

void
ps_check_event_packet(struct hci_uart *hu, struct sk_buff *skb)
{
	struct hci_event_hdr *hdr = (void *)skb->data;
	struct hci_ev_cmd_complete *ev = NULL;
	u8 event = hdr->evt;
	u16 opcode;
	u8 status = 0;
	struct ps_data *psdata = hu->psdata;

	if (!psdata->send_cmd)
		return;
	if (event == HCI_EV_CMD_COMPLETE) {
		ev = (void *)(skb->data + sizeof(struct hci_event_hdr));
		opcode = __le16_to_cpu(ev->opcode);
		switch (opcode) {
		case HCI_OP_AUTO_SLEEP_MODE:
			status = *((u8 *) ev +
				   sizeof(struct hci_ev_cmd_complete));
			if (!status)
				psdata->cur_psmode = psdata->ps_mode;
			else
				psdata->ps_mode = psdata->cur_psmode;
			psdata->send_cmd &= ~SEND_AUTO_SLEEP_MODE_CMD;
			if (psdata->cur_psmode == PS_MODE_ENABLE)
				ps_start_timer(hu);
			else
				ps_wakeup(hu);
			PRINTM(CMD, "status=%d,ps_mode=%d\n", status,
			       psdata->cur_psmode);
			break;
		case HCI_OP_WAKEUP_METHOD:
			status = *((u8 *) ev +
				   sizeof(struct hci_ev_cmd_complete));
			psdata->send_cmd &= ~SEND_WAKEUP_METHOD_CMD;
			if (!status)
				psdata->cur_wakeupmode = psdata->wakeupmode;
			else
				psdata->wakeupmode = psdata->cur_wakeupmode;
			PRINTM(CMD, "status=%d,wakeupmode=%d\n", status,
			       psdata->cur_wakeupmode);
			break;
		default:
			break;
		}
	}
	return;
}

/** This function init power save specific proc entries  */
int
ps_proc_init(struct hci_uart *hu)
{
	u8 ret = 0;
	struct proc_dir_entry *entry;

	if (proc_bt && !hu->proc_bt) {
		strcpy(hu->proc_name, hu->hdev->name);
		hu->proc_bt = proc_mkdir(hu->proc_name, proc_bt);
		if (!hu->proc_bt) {
			PRINTM(ERROR, "Could not mkdir %s!\n", hu->proc_name);
			ret = -1;
			goto done;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
		entry = proc_create_data("config", S_IFREG | DEFAULT_FILE_PERM,
					 hu->proc_bt, &proc_rw_ops, hu);
		if (entry == NULL)
#else
		entry = create_proc_entry("config", S_IFREG | DEFAULT_FILE_PERM,
					  hu->proc_bt);
		if (entry) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
			entry->owner = THIS_MODULE;
#endif
			entry->proc_fops = &proc_rw_ops;
			entry->data = hu;
		} else
#endif
			PRINTM(ERROR, "MUART: Fail to create proc\n");
	}
done:
	return ret;
}

/** remove power save proc files */
void
ps_proc_remove(struct hci_uart *hu)
{
	if (proc_bt && hu->proc_bt) {
		remove_proc_entry("config", hu->proc_bt);
		remove_proc_entry(hu->proc_name, proc_bt);
		hu->proc_bt = NULL;
	}
	return;
}

#else /* CONFIG_MULTI_CARD_PS */
void
ps_send_char_complete(u8 ch)
{
	PRINTM(CMD, "Send char %c done\n", ch);
	if (g_data.ps_mode == PS_MODE_ENABLE) {
		if (ch == MRVL_ENTER_PS_CHAR)
			g_data.ps_state = PS_STATE_SLEEP;
		else if (ch == MRVL_EXIT_PS_CHAR)
			g_data.ps_state = PS_STATE_AWAKE;
	}
}

void
ps_init_timer(struct tty_struct *tty)
{
	PRINTM(MSG, "ps_init_timer...\n");
	g_data.timer_on = 0;
	g_data.tty = tty;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
	timer_setup(&g_data.ps_timer, ps_timeout_func, 0);
#else
	init_timer(&g_data.ps_timer);
	g_data.ps_timer.function = ps_timeout_func;
	g_data.ps_timer.data = (unsigned long)&g_data;
#endif
	return;
}

void
ps_start_timer(void)
{
	if (g_data.cur_psmode == PS_MODE_ENABLE) {
		g_data.timer_on = 1;
		mod_timer(&g_data.ps_timer,
			  jiffies + (g_data.interval * HZ) / 1000);
	}
}

void
ps_cancel_timer(void)
{
	flush_scheduled_work();
	if (g_data.timer_on)
		del_timer(&g_data.ps_timer);
	if ((g_data.cur_psmode == PS_MODE_ENABLE) &&
	    (g_data.cur_wakeupmode == WAKEUP_METHOD_BREAK)) {
		// set_break off
		set_break(g_data.tty, 0);
	}
	g_data.tty = NULL;
	return;
}

int
ps_wakeup(void)
{
	if (g_data.ps_state == PS_STATE_AWAKE)
		return 0;
	g_data.ps_cmd = PS_CMD_EXIT_PS;
	schedule_work(&g_data.work);
	return 1;
}

void
ps_init(void)
{
	struct hci_uart *hu = NULL;
	int mode = 0;
	struct ktermios old_termios;
	PRINTM(MSG, "ps_init...\n");
	if (!g_data.tty)
		return;
	if (1 != get_cts(g_data.tty)) {
		/* firmware is sleeping */
		mode = g_data.cur_wakeupmode;
		if (mode == WAKEUP_METHOD_INVALID)
			mode = wakeupmode;
		switch (mode) {
		case WAKEUP_METHOD_BREAK:
			// set RTS
			set_rts(g_data.tty, 1);
			// break on
			set_break(g_data.tty, 1);
			// break off
			set_break(g_data.tty, 0);
			mdelay(5);
			break;
		case WAKEUP_METHOD_DTR:
			// set RTS
			set_rts(g_data.tty, 1);
			set_dtr(g_data.tty, 0);
			set_dtr(g_data.tty, 1);
			mdelay(5);
			break;
		case WAKEUP_METHOD_GPIO:
			/* todo: implement gpio wakeup method */
			break;
		default:
			break;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
		old_termios = g_data.tty->termios;
		g_data.tty->termios.c_cflag &= ~CRTSCTS;	/* Clear the
								   flow control
								 */
		g_data.TTY_FUNC->set_termios(g_data.tty, &old_termios);
		old_termios = g_data.tty->termios;
		g_data.tty->termios.c_cflag |= CRTSCTS;	/* Enable the flow
							   control */
		g_data.TTY_FUNC->set_termios(g_data.tty, &old_termios);
#else
		old_termios = *(g_data.tty->termios);
		g_data.tty->termios->c_cflag &= ~CRTSCTS;	/* Clear the
								   flow control
								 */
		g_data.TTY_FUNC->set_termios(g_data.tty, &old_termios);
		old_termios = *(g_data.tty->termios);
		g_data.tty->termios->c_cflag |= CRTSCTS;	/* Enable the
								   flow control
								 */
		g_data.TTY_FUNC->set_termios(g_data.tty, &old_termios);
#endif
	}

	g_data.send_cmd = 0;
	hu = (void *)g_data.tty->disc_data;
	if (0 == is_device_ready(hu)) {

		if (g_data.cur_wakeupmode != g_data.wakeupmode) {
			g_data.send_cmd |= SEND_WAKEUP_METHOD_CMD;
			send_wakeup_method_cmd(g_data.wakeupmode, hu);
		}
		if (g_data.cur_psmode != g_data.ps_mode) {
			g_data.send_cmd |= SEND_AUTO_SLEEP_MODE_CMD;
			send_ps_cmd(g_data.ps_mode, hu);
		}
	}
}

void
ps_check_event_packet(struct sk_buff *skb)
{
	struct hci_event_hdr *hdr = (void *)skb->data;
	struct hci_ev_cmd_complete *ev = NULL;
	u8 event = hdr->evt;
	u16 opcode;
	u8 status = 0;
	if (!g_data.send_cmd)
		return;
	if (event == HCI_EV_CMD_COMPLETE) {
		ev = (void *)(skb->data + sizeof(struct hci_event_hdr));
		opcode = __le16_to_cpu(ev->opcode);
		switch (opcode) {
		case HCI_OP_AUTO_SLEEP_MODE:
			status = *((u8 *) ev +
				   sizeof(struct hci_ev_cmd_complete));
			if (!status)
				g_data.cur_psmode = g_data.ps_mode;
			else
				g_data.ps_mode = g_data.cur_psmode;
			g_data.send_cmd &= ~SEND_AUTO_SLEEP_MODE_CMD;
			if (g_data.cur_psmode == PS_MODE_ENABLE)
				ps_start_timer();
			else
				ps_wakeup();
			PRINTM(CMD, "status=%d,ps_mode=%d\n", status,
			       g_data.cur_psmode);
			break;
		case HCI_OP_WAKEUP_METHOD:
			status = *((u8 *) ev +
				   sizeof(struct hci_ev_cmd_complete));
			g_data.send_cmd &= ~SEND_WAKEUP_METHOD_CMD;
			if (!status)
				g_data.cur_wakeupmode = g_data.wakeupmode;
			else
				g_data.wakeupmode = g_data.cur_wakeupmode;
			PRINTM(CMD, "status=%d,wakeupmode=%d\n", status,
			       g_data.cur_wakeupmode);
			break;
		default:
			break;
		}
	}
	return;
}
#endif /* CONFIG_MULTI_CARD_PS */

module_param(ps_mode, int, 0);
MODULE_PARM_DESC(ps_mode, "ps mode: 0 disable ps mode, 1 enable ps mode");
module_param(wakeupmode, int, 0);
MODULE_PARM_DESC(wakeupmode, "wakeup mode");
module_param(h2c_wakeup_gpio_pin, int, 0);
MODULE_PARM_DESC(h2c_wakeup_gpio_pin, "h2c wakeup gpio pin");
module_param(c2h_wakeup_mode, int, 0);
MODULE_PARM_DESC(c2h_wakeup_mode, "c2h wakeup mode");
module_param(c2h_wakeup_gpio_pin, int, 0);
MODULE_PARM_DESC(c2h_wakeup_gpio_pin, "c2h wakeup gpio pin");
module_param(drvdbg, uint, 0660);
MODULE_PARM_DESC(drvdbg, "Driver Debug");
