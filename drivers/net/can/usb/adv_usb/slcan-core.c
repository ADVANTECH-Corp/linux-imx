/*
 * slcan.c - serial line CAN interface driver (using tty line discipline)
 *
 * This file is derived from linux/drivers/net/slip/slip.c and got
 * inspiration from linux/drivers/net/can/can327.c for the rework made
 * on the line discipline code.
 *
 * slip.c Authors  : Laurence Culhane <loz@holmes.demon.co.uk>
 *                   Fred N. van Kempen <waltje@uwalt.nl.mugnet.org>
 * slcan.c Author  : Oliver Hartkopp <socketcan@hartkopp.net>
 * can327.c Author : Max Staudt <max-linux@enpas.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see http://www.gnu.org/licenses/gpl.html
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "kcompat.h"
#include "slcan.h"

MODULE_ALIAS_LDISC(N_SLCAN);
MODULE_DESCRIPTION("serial line CAN interface");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Oliver Hartkopp <socketcan@hartkopp.net>");
MODULE_AUTHOR("Dario Binacchi <dario.binacchi@amarulasolutions.com>");

#define SLCAN_CMD_LEN 1
#define SLCAN_SFF_ID_LEN 3
#define SLCAN_EFF_ID_LEN 8
#define SLCAN_STATE_LEN 1
#define SLCAN_STATE_BE_RXCNT_LEN 3
#define SLCAN_STATE_BE_TXCNT_LEN 3
#define SLCAN_STATE_FRAME_LEN       (1 + SLCAN_CMD_LEN + \
				     SLCAN_STATE_BE_RXCNT_LEN + \
				     SLCAN_STATE_BE_TXCNT_LEN)

bool slcan_err_rst_on_open(struct net_device *ndev)
{
	struct slcan *sl = netdev_priv(ndev);

	return !!test_bit(CF_ERR_RST, &sl->cmd_flags);
}

int slcan_enable_err_rst_on_open(struct net_device *ndev, bool on)
{
	struct slcan *sl = netdev_priv(ndev);

	if (netif_running(ndev))
		return -EBUSY;

	if (on)
		set_bit(CF_ERR_RST, &sl->cmd_flags);
	else
		clear_bit(CF_ERR_RST, &sl->cmd_flags);

	return 0;
}

/*************************************************************************
 *			SLCAN ENCAPSULATION FORMAT			 *
 *************************************************************************/

/* A CAN frame has a can_id (11 bit standard frame format OR 29 bit extended
 * frame format) a data length code (len) which can be from 0 to 8
 * and up to <len> data bytes as payload.
 * Additionally a CAN frame may become a remote transmission frame if the
 * RTR-bit is set. This causes another ECU to send a CAN frame with the
 * given can_id.
 *
 * The SLCAN ASCII representation of these different frame types is:
 * <type> <id> <dlc> <data>*
 *
 * Extended frames (29 bit) are defined by capital characters in the type.
 * RTR frames are defined as 'r' types - normal frames have 't' type:
 * t => 11 bit data frame
 * r => 11 bit RTR frame
 * T => 29 bit data frame
 * R => 29 bit RTR frame
 *
 * b => 11 bit data CAN-FD frame(with BRS)
 * d => 11 bit data CAN-FD frame(without BRS)
 *
 * The <id> is 3 (standard) or 8 (extended) bytes in ASCII Hex (base64).
 * The <dlc> is a one byte ASCII number ('0' - '8')
 * The <data> section has at much ASCII Hex bytes as defined by the <dlc>
 *
 * Examples:
 *
 * t1230 : can_id 0x123, len 0, no data
 * t4563112233 : can_id 0x456, len 3, data 0x11 0x22 0x33
 * T12ABCDEF2AA55 : extended can_id 0x12ABCDEF, len 2, data 0xAA 0x55
 * r1230 : can_id 0x123, len 0, no data, remote transmission request
 *
 * d123a11223344556677881122334455667788 : can_id 0x123, can_dlc: a(16byte),
 * 						data: 0x11223344556677881122334455667788,
 * 						can-fd, not use BRS.
 *
 */

static int decode_can_len(u8 len, u16 proto)
{
	if (len >= '0' && len < '9') {
		return (len -= '0');
	}

	if (proto != ETH_P_CANFD) {
		// error protocol
		return -1;
	}

	// convert to lower case for len
	len |= 0x20;

	// CAN-FD Package DLC
	switch (len) {
	case '9':
		len = 12;
		break;
	case 'a':
		len = 16;
		break;
	case 'b':
		len = 20;
		break;
	case 'c':
		len = 24;
		break;
	case 'd':
		len = 32;
		break;
	case 'e':
		len = 48;
		break;
	case 'f':
		len = 64;
		break;
	default:
		return -1;
	}

	return len;
}

static u8 encode_can_len(u8 len, u16 proto)
{
	if (len <= 8) {
		return (len + '0');
	}

	if (proto != ETH_P_CANFD) {
		// error protocol
		return '0';
	}


	// CAN-FD Package DLC
	switch (len) {
	case 12:
		len = '9';
		break;
	case 16:
		len = 'a';
		break;
	case 20:
		len = 'b';
		break;
	case 24:
		len = 'c';
		break;
	case 32:
		len = 'd';
		break;
	case 48:
		len = 'e';
		break;
	case 64:
		len = 'f';
		break;
	default:
		len = '0';
		break;
	}

	return len;
}

/*************************************************************************
 *			STANDARD SLCAN DECAPSULATION			 *
 *************************************************************************/

/* Send one completely decapsulated can_frame to the network layer */
static void slcan_bump_frame(struct slcan *sl)
{
	struct sk_buff *skb;
	struct canfd_frame *cf;
	int i, tmp;
	u32 tmpid;
	u16 CAN_PROTO = ETH_P_CAN;
	char *cmd = sl->rbuff;

	//use canfd frame
	if (sl->fd_on) {
		skb = alloc_canfd_skb(sl->dev, &cf);
	} else {
		skb = alloc_can_skb(sl->dev, (struct can_frame **)&cf);
	}

	if (unlikely(!skb)) {
		sl->dev->stats.rx_dropped++;
		return;
	}

	switch (*cmd) {
	case 'b':
		cf->flags |= CANFD_BRS;
        fallthrough;
	case 'd':
		CAN_PROTO = ETH_P_CANFD;
        fallthrough;
	case 'r':
		if (!sl->fd_on) {
			cf->can_id = CAN_RTR_FLAG;
		}
        fallthrough;
	case 't':
		/* store dlc ASCII value and terminate SFF CAN ID string */
		cf->len = sl->rbuff[SLCAN_CMD_LEN + SLCAN_SFF_ID_LEN];
		sl->rbuff[SLCAN_CMD_LEN + SLCAN_SFF_ID_LEN] = 0;
		/* point to payload data behind the dlc */
		cmd += SLCAN_CMD_LEN + SLCAN_SFF_ID_LEN + 1;
		break;
	case 'B':
		cf->flags |= CANFD_BRS;
        fallthrough;
	case 'D':
		CAN_PROTO = ETH_P_CANFD;
        fallthrough;
	case 'R':
		if (!sl->fd_on) {
			cf->can_id = CAN_RTR_FLAG;
		}
        fallthrough;
	case 'T':
		cf->can_id |= CAN_EFF_FLAG;
		/* store dlc ASCII value and terminate EFF CAN ID string */
		cf->len = sl->rbuff[SLCAN_CMD_LEN + SLCAN_EFF_ID_LEN];
		sl->rbuff[SLCAN_CMD_LEN + SLCAN_EFF_ID_LEN] = 0;
		/* point to payload data behind the dlc */
		cmd += SLCAN_CMD_LEN + SLCAN_EFF_ID_LEN + 1;
		break;
	default:
		goto decode_failed;
	}

	if (kstrtou32(sl->rbuff + SLCAN_CMD_LEN, 16, &tmpid))
		goto decode_failed;

	cf->can_id |= tmpid;

	/* get len from sanitized ASCII value */
	tmp = decode_can_len(cf->len, ETH_P_CANFD);
	if (tmp < 0)
		goto decode_failed;

	cf->len = (u8)tmp;

	/* RTR frames may have a dlc > 0 but they never have any data bytes */
	if (!(cf->can_id & CAN_RTR_FLAG)) {
		for (i = 0; i < cf->len; i++) {
			tmp = hex_to_bin(*cmd++);
			if (tmp < 0)
				goto decode_failed;

			cf->data[i] = (tmp << 4);
			tmp = hex_to_bin(*cmd++);
			if (tmp < 0)
				goto decode_failed;

			cf->data[i] |= tmp;
		}
	}

	sl->dev->stats.rx_packets++;
	if (!(cf->can_id & CAN_RTR_FLAG))
		sl->dev->stats.rx_bytes += cf->len;

	netif_receive_skb(skb);
	return;

decode_failed:
	sl->dev->stats.rx_errors++;
	dev_kfree_skb(skb);
}

/* A change state frame must contain state info and receive and transmit
 * error counters.
 *
 * Examples:
 *
 * sb256256 : state bus-off: rx counter 256, tx counter 256
 * sa057033 : state active, rx counter 57, tx counter 33
 */
static void slcan_bump_state(struct slcan *sl)
{
	struct net_device *dev = sl->dev;
	struct sk_buff *skb;
	struct can_frame *cf;
	char *cmd = sl->rbuff;
	u32 rxerr, txerr;
	enum can_state state, rx_state, tx_state;

	switch (cmd[1]) {
	case 'a':
		state = CAN_STATE_ERROR_ACTIVE;
		break;
	case 'w':
		state = CAN_STATE_ERROR_WARNING;
		break;
	case 'p':
		state = CAN_STATE_ERROR_PASSIVE;
		break;
	case 'b':
		state = CAN_STATE_BUS_OFF;
		break;
	default:
		return;
	}

	if (state == sl->can.state || sl->rcount < SLCAN_STATE_FRAME_LEN)
		return;

	cmd += SLCAN_STATE_BE_RXCNT_LEN + SLCAN_CMD_LEN + 1;
	cmd[SLCAN_STATE_BE_TXCNT_LEN] = 0;
	if (kstrtou32(cmd, 10, &txerr))
		return;

	*cmd = 0;
	cmd -= SLCAN_STATE_BE_RXCNT_LEN;
	if (kstrtou32(cmd, 10, &rxerr))
		return;

	skb = alloc_can_err_skb(dev, &cf);

	tx_state = txerr >= rxerr ? state : 0;
	rx_state = txerr <= rxerr ? state : 0;
	can_change_state(dev, cf, tx_state, rx_state);

	if (state == CAN_STATE_BUS_OFF) {
		can_bus_off(dev);
	} else if (skb) {
		cf->can_id |= CAN_ERR_CNT;
		cf->data[6] = txerr;
		cf->data[7] = rxerr;
	}

	if (skb)
		netif_receive_skb(skb);
}

/* An error frame can contain more than one type of error.
 *
 * Examples:
 *
 * e1a : len 1, errors: ACK error
 * e3bcO: len 3, errors: Bit0 error, CRC error, Tx overrun error
 */
static void slcan_bump_err(struct slcan *sl)
{
	struct net_device *dev = sl->dev;
	struct sk_buff *skb;
	struct can_frame *cf;
	char *cmd = sl->rbuff;
	bool rx_errors = false, tx_errors = false, rx_over_errors = false;
	int i, len;

	/* get len from sanitized ASCII value */
	len = cmd[1];
	if (len >= '0' && len < '9')
		len -= '0';
	else
		return;

	if ((len + SLCAN_CMD_LEN + 1) > sl->rcount)
		return;

	skb = alloc_can_err_skb(dev, &cf);

	if (skb)
		cf->can_id |= CAN_ERR_PROT | CAN_ERR_BUSERROR;

	cmd += SLCAN_CMD_LEN + 1;
	for (i = 0; i < len; i++, cmd++) {
		switch (*cmd) {
		case 'a':
			netdev_dbg(dev, "ACK error\n");
			tx_errors = true;
			if (skb) {
				cf->can_id |= CAN_ERR_ACK;
				cf->data[3] = CAN_ERR_PROT_LOC_ACK;
			}

			break;
		case 'b':
			netdev_dbg(dev, "Bit0 error\n");
			tx_errors = true;
			if (skb)
				cf->data[2] |= CAN_ERR_PROT_BIT0;

			break;
		case 'B':
			netdev_dbg(dev, "Bit1 error\n");
			tx_errors = true;
			if (skb)
				cf->data[2] |= CAN_ERR_PROT_BIT1;

			break;
		case 'c':
			netdev_dbg(dev, "CRC error\n");
			rx_errors = true;
			if (skb) {
				cf->data[2] |= CAN_ERR_PROT_BIT;
				cf->data[3] = CAN_ERR_PROT_LOC_CRC_SEQ;
			}

			break;
		case 'f':
			netdev_dbg(dev, "Form Error\n");
			rx_errors = true;
			if (skb)
				cf->data[2] |= CAN_ERR_PROT_FORM;

			break;
		case 'o':
			netdev_dbg(dev, "Rx overrun error\n");
			rx_over_errors = true;
			rx_errors = true;
			if (skb) {
				cf->can_id |= CAN_ERR_CRTL;
				cf->data[1] = CAN_ERR_CRTL_RX_OVERFLOW;
			}

			break;
		case 'O':
			netdev_dbg(dev, "Tx overrun error\n");
			tx_errors = true;
			if (skb) {
				cf->can_id |= CAN_ERR_CRTL;
				cf->data[1] = CAN_ERR_CRTL_TX_OVERFLOW;
			}

			break;
		case 's':
			netdev_dbg(dev, "Stuff error\n");
			rx_errors = true;
			if (skb)
				cf->data[2] |= CAN_ERR_PROT_STUFF;

			break;
		default:
			if (skb)
				dev_kfree_skb(skb);

			return;
		}
	}

	if (rx_errors)
		dev->stats.rx_errors++;

	if (rx_over_errors)
		dev->stats.rx_over_errors++;

	if (tx_errors)
		dev->stats.tx_errors++;

	if (skb)
		netif_receive_skb(skb);
}

static void slcan_bump(struct slcan *sl)
{
	switch (sl->rbuff[0]) {
	case 'b':
        fallthrough;
	case 'd':
	    sl->fd_on = true;
        fallthrough;
	case 'r':
        fallthrough;
	case 't':
		return slcan_bump_frame(sl);
	case 'B':
        fallthrough;
	case 'D':
		sl->fd_on = true;
        fallthrough;
	case 'R':
        fallthrough;
	case 'T':
		return slcan_bump_frame(sl);
	case 'e':
		return slcan_bump_err(sl);
	case 's':
		return slcan_bump_state(sl);
	default:
		return;
	}
}

/* parse tty input stream */
static void slcan_unesc(struct slcan *sl, unsigned char s)
{
	if ((s == '\r') || (s == '\a')) { /* CR or BEL ends the pdu */
		if (!test_and_clear_bit(SLF_ERROR, &sl->flags) &&
		    sl->rcount > 4)
			slcan_bump(sl);

		sl->rcount = 0;
	} else {
		if (!test_bit(SLF_ERROR, &sl->flags))  {
			if (sl->rcount < SLCAN_MTU)  {
				sl->rbuff[sl->rcount++] = s;
				return;
			}

			sl->dev->stats.rx_over_errors++;
			set_bit(SLF_ERROR, &sl->flags);
		}
	}
}

/*************************************************************************
 *			STANDARD SLCAN ENCAPSULATION			 *
 *************************************************************************/

/* Encapsulate one can_frame and stuff into a TTY queue. */
static void slcan_encaps(struct slcan *sl, struct canfd_frame *cf)
{
	int actual, i;
	unsigned char *pos;
	unsigned char *endpos;
	u16 CAN_PROTO = ETH_P_CAN;
	canid_t id = cf->can_id;

	pos = sl->xbuff;

	if (cf->can_id & CAN_RTR_FLAG)
		*pos = 'R'; /* becomes 'r' in standard frame format (SFF) */
	else
		*pos = 'T'; /* becomes 't' in standard frame format (SSF) */

	if (sl->fd_on) {
		CAN_PROTO = ETH_P_CANFD;
		*pos = 'D';
		if (cf->flags & CANFD_BRS) {
			*pos = 'B';
		}
	}

	/* determine number of chars for the CAN-identifier */
	endpos = pos + SLCAN_EFF_ID_LEN;
	id &= CAN_EFF_MASK;

	if (!(cf->can_id & CAN_EFF_FLAG)) {
		id &= CAN_SFF_MASK;
		*pos |= 0x20; /* convert R/T to lower case for SFF */
		endpos = pos + SLCAN_SFF_ID_LEN;
	}

	/* build 3 (SFF) or 8 (EFF) digit CAN identifier */
	pos++;
	while (endpos >= pos) {
		*endpos-- = hex_asc_upper[id & 0xf];
		id >>= 4;
	}

	pos += (cf->can_id & CAN_EFF_FLAG) ?
		SLCAN_EFF_ID_LEN : SLCAN_SFF_ID_LEN;

	*pos++ = encode_can_len(cf->len, CAN_PROTO);

	/* RTR frames may have a dlc > 0 but they never have any data bytes */
	if (!(cf->can_id & CAN_RTR_FLAG)) {
		for (i = 0; i < cf->len; i++)
			pos = hex_byte_pack_upper(pos, cf->data[i]);

		sl->dev->stats.tx_bytes += cf->len;
	}

	*pos++ = '\r';

	/* Order of next two lines is *very* important.
	 * When we are sending a little amount of data,
	 * the transfer may be completed inside the ops->write()
	 * routine, because it's running with interrupts enabled.
	 * In this case we *never* got WRITE_WAKEUP event,
	 * if we did not request it before write operation.
	 *       14 Oct 1994  Dmitry Gorodchanin.
	 */
	set_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
	actual = sl->tty->ops->write(sl->tty, sl->xbuff, pos - sl->xbuff);
	sl->xleft = (pos - sl->xbuff) - actual;
	sl->xhead = sl->xbuff + actual;
}

/* Write out any remaining transmit buffer. Scheduled when tty is writable */
static void slcan_transmit(struct work_struct *work)
{
	struct slcan *sl = container_of(work, struct slcan, tx_work);
	int actual;

	spin_lock_bh(&sl->lock);
	/* First make sure we're connected. */
	if (unlikely(!netif_running(sl->dev)) &&
	    likely(!test_bit(SLF_XCMD, &sl->flags))) {
		spin_unlock_bh(&sl->lock);
		return;
	}

	if (sl->xleft <= 0)  {
		if (unlikely(test_bit(SLF_XCMD, &sl->flags))) {
			clear_bit(SLF_XCMD, &sl->flags);
			clear_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
			spin_unlock_bh(&sl->lock);
			wake_up(&sl->xcmd_wait);
			return;
		}

		/* Now serial buffer is almost free & we can start
		 * transmission of another packet
		 */
		sl->dev->stats.tx_packets++;
		clear_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
		spin_unlock_bh(&sl->lock);
		netif_wake_queue(sl->dev);
		return;
	}

	actual = sl->tty->ops->write(sl->tty, sl->xhead, sl->xleft);
	sl->xleft -= actual;
	sl->xhead += actual;
	spin_unlock_bh(&sl->lock);
}

/* Called by the driver when there's room for more data.
 * Schedule the transmit.
 */
static void slcan_write_wakeup(struct tty_struct *tty)
{
	struct slcan *sl = (struct slcan *)tty->disc_data;

	schedule_work(&sl->tx_work);
}

/* Send a can_frame to a TTY queue. */
static netdev_tx_t slcan_netdev_xmit(struct sk_buff *skb,
				     struct net_device *dev)
{
	struct slcan *sl = netdev_priv(dev);

	if (can_dropped_invalid_skb(dev, skb))
		return NETDEV_TX_OK;

	if (skb->len == CANFD_MTU) {
		//turn on the canfd mode
		sl->fd_on = true;
	}

	spin_lock(&sl->lock);
	if (!netif_running(dev))  {
		spin_unlock(&sl->lock);
		netdev_warn(dev, "xmit: iface is down\n");
		goto out;
	}
	if (!sl->tty) {
		spin_unlock(&sl->lock);
		goto out;
	}

	netif_stop_queue(sl->dev);
	slcan_encaps(sl, (struct canfd_frame *)skb->data); /* encaps & send */
	spin_unlock(&sl->lock);

	skb_tx_timestamp(skb);

out:
	kfree_skb(skb);
	return NETDEV_TX_OK;
}

/******************************************
 *   Routines looking at netdevice side.
 ******************************************/

static int slcan_transmit_cmd(struct slcan *sl, const unsigned char *cmd)
{
	int ret, actual, n;

	spin_lock(&sl->lock);
	if (!sl->tty) {
		spin_unlock(&sl->lock);
		return -ENODEV;
	}

	n = scnprintf(sl->xbuff, sizeof(sl->xbuff), "%s", cmd);
	set_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
	actual = sl->tty->ops->write(sl->tty, sl->xbuff, n);
	sl->xleft = n - actual;
	sl->xhead = sl->xbuff + actual;
	set_bit(SLF_XCMD, &sl->flags);
	spin_unlock(&sl->lock);
	ret = wait_event_interruptible_timeout(sl->xcmd_wait,
					       !test_bit(SLF_XCMD, &sl->flags),
					       HZ);
	clear_bit(SLF_XCMD, &sl->flags);
	if (ret == -ERESTARTSYS)
		return ret;

	if (ret == 0)
		return -ETIMEDOUT;

	return 0;
}

/* Netdevice UP -> DOWN routine */
static int slcan_netdev_close(struct net_device *dev)
{
	struct slcan *sl = netdev_priv(dev);
	int err;

    //disable can when set socket can to down
	err = slcan_transmit_cmd(sl, "C\r");
	if (err)
		netdev_warn(dev, "failed to send close command 'C\\r'\n");

	/* TTY discipline is running. */
	clear_bit(TTY_DO_WRITE_WAKEUP, &sl->tty->flags);
	flush_work(&sl->tx_work);

	netif_stop_queue(dev);
	sl->rcount   = 0;
	sl->xleft    = 0;
	close_candev(dev);
	sl->can.state = CAN_STATE_STOPPED;
	if (sl->can.bittiming.bitrate == CAN_BITRATE_UNKNOWN)
		sl->can.bittiming.bitrate = CAN_BITRATE_UNSET;

	return 0;
}

/* Netdevice DOWN -> UP routine */
static int slcan_netdev_open(struct net_device *dev)
{
	struct slcan *sl = netdev_priv(dev);
	unsigned char cmd[SLCAN_MTU];
	int err;

	/* The baud rate is not set with the command
	 * `ip link set <iface> type can bitrate <baud>' and therefore
	 * can.bittiming.bitrate is CAN_BITRATE_UNSET (0), causing
	 * open_candev() to fail. So let's set to a fake value.
	 */
	if (sl->can.bittiming.bitrate == CAN_BITRATE_UNSET)
		sl->can.bittiming.bitrate = CAN_BITRATE_UNKNOWN;

	err = open_candev(dev);
	if (err) {
		netdev_err(dev, "failed to open can device\n");
		return err;
	}

	if (sl->can.bittiming.bitrate != CAN_BITRATE_UNKNOWN &&
		sl->can.data_bittiming.bitrate != CAN_BITRATE_UNKNOWN ) {

		/* The CAN framework has already validate the bitrate value,
		 * so we can avoid to check if `s' has been properly set.
		 */
		snprintf(cmd, sizeof(cmd), "C\r");
		err = slcan_transmit_cmd(sl, cmd);
		if (err) {
			netdev_err(dev, "failed to send close command 'C\\r'\n");
			goto cmd_transmit_failed;
		}

		if (test_bit(CF_ERR_RST, &sl->cmd_flags)) {
			err = slcan_transmit_cmd(sl, "F\r");
			if (err) {
				netdev_err(dev, "failed to send error command 'F\\r'\n");
				goto cmd_transmit_failed;
			}
		}

		if (sl->can.ctrlmode & CAN_CTRLMODE_LISTENONLY) {
			err = slcan_transmit_cmd(sl, "L\r");
			if (err) {
				netdev_err(dev, "failed to send listen-only command 'L\\r'\n");
				goto cmd_transmit_failed;
			}
		} else {
			err = slcan_transmit_cmd(sl, "O\r");
			if (err) {
				netdev_err(dev, "failed to send open command 'O\\r'\n");
				goto cmd_transmit_failed;
			}
		}
	}

	sl->can.state = CAN_STATE_ERROR_ACTIVE;
	netif_start_queue(dev);
	return 0;

cmd_transmit_failed:
	close_candev(dev);
	return err;
}

static const struct net_device_ops slcan_netdev_ops = {
	.ndo_open               = slcan_netdev_open,
	.ndo_stop               = slcan_netdev_close,
	.ndo_start_xmit         = slcan_netdev_xmit,
	.ndo_change_mtu         = can_change_mtu,
};

/******************************************
 *  Routines looking at TTY side.
 ******************************************/

/* Handle the 'receiver data ready' interrupt.
 * This function is called by the 'tty_io' module in the kernel when
 * a block of SLCAN data has been received, which can now be decapsulated
 * and sent on to some IP layer for further processing. This will not
 * be re-entered while running but other ldisc functions may be called
 * in parallel
 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0) )
static void slcan_receive_buf(struct tty_struct *tty,
			      const unsigned char *cp, char *fp,
			      int count)
#elif ( LINUX_VERSION_CODE < KERNEL_VERSION(6,6,0) )
static void slcan_receive_buf(struct tty_struct *tty,
			      const unsigned char *cp, const char *fp,
			      int count)
#else
static void slcan_receive_buf(struct tty_struct *tty,
			      const u8 *cp, const u8 *fp,
			      size_t count)
#endif
{
	struct slcan *sl = (struct slcan *)tty->disc_data;

	if (!netif_running(sl->dev))
		return;

	/* Read the characters out of the buffer */
	while (count--) {
		if (fp && *fp++) {
			if (!test_and_set_bit(SLF_ERROR, &sl->flags))
				sl->dev->stats.rx_errors++;
			cp++;
			continue;
		}
		slcan_unesc(sl, *cp++);
	}
}

/* gd32c103 can bittiming */
static const struct can_bittiming_const gd_bittiming_const = {
    .name = "slcan",
    .tseg1_min = 0x01,
    .tseg1_max = 0x80,
    .tseg2_min = 0x01,
    .tseg2_max = 0x20,
    .sjw_max = 0x20,
    .brp_min = 0x01,
    .brp_max = 0x400,
    .brp_inc = 0x01,
};

/* gd32c103 can data bittiming */
static const struct can_bittiming_const gd_data_bittiming_const = {
    .name = "slcan",
    .tseg1_min = 0x01,
    .tseg1_max = 0x10,
    .tseg2_min = 0x01,
    .tseg2_max = 0x08,
    .sjw_max = 0x08,
    .brp_min = 0x01,
    .brp_max = 0x400,
    .brp_inc = 0x01,
};

static int slcan_set_bittiming(struct net_device *dev) {
	struct slcan *sl;
	int err;
	unsigned char cmd[SLCAN_MTU];
	sl = netdev_priv(dev);
	
	/* 'P' command to set can bittiming
	 * example P0001000600010375
	 * 0001: sjw = 1
	 * phase_seg1 = 6
	 * phase_seg2 = 1
	 * prescaler = 375 */
	snprintf(cmd, sizeof(cmd), "P%04d%04d%04d%04d\r",
	 sl->can.bittiming.sjw,
	 sl->can.bittiming.phase_seg1 + sl->can.bittiming.prop_seg,
	 sl->can.bittiming.phase_seg2,
	 sl->can.bittiming.brp);
	err = slcan_transmit_cmd(sl, cmd);
	if (err) {
		netdev_err(dev, "failed to send bittiming parameter cmd\n");
	}
	return err;
}

static int slcan_set_data_bittiming(struct net_device *dev) {
	struct slcan *sl;
	int err;
	unsigned char cmd[SLCAN_MTU];
	sl = netdev_priv(dev);
	
	/* 'p' command to set can data bittiming
	 * example p0001000600010375
	 * 0001: dsjw = 1
	 * data phase_seg1 = 6
	 * data phase_seg2 = 1
	 * data prescaler = 375*/
	snprintf(cmd, sizeof(cmd), "p%04d%04d%04d%04d\r",
	 sl->can.data_bittiming.sjw,
	 sl->can.data_bittiming.phase_seg1 + sl->can.data_bittiming.prop_seg,
	 sl->can.data_bittiming.phase_seg2,
	 sl->can.data_bittiming.brp);
	err = slcan_transmit_cmd(sl, cmd);
	if (err) {
		netdev_err(dev, "failed to send data bittiming parameter cmd\n");
	}
	return err;
}

/* Open the high-level part of the SLCAN channel.
 * This function is called by the TTY module when the
 * SLCAN line discipline is called for.
 *
 * Called in process context serialized from other ldisc calls.
 */
static int slcan_open(struct tty_struct *tty)
{
	struct net_device *dev;
	struct slcan *sl;
	int err;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!tty->ops->write)
		return -EOPNOTSUPP;

	dev = alloc_candev(sizeof(*sl), 1);
	if (!dev)
		return -ENFILE;

	sl = netdev_priv(dev);

	/* Configure TTY interface */
	tty->receive_room = 65536; /* We don't flow control */
	sl->rcount = 0;
	sl->xleft = 0;
	spin_lock_init(&sl->lock);
	INIT_WORK(&sl->tx_work, slcan_transmit);
	init_waitqueue_head(&sl->xcmd_wait);

	/*virtual support fd on via 'ip set' command*/
	sl->can.clock.freq = 60000000;
	sl->can.ctrlmode_supported = CAN_CTRLMODE_LOOPBACK |
		CAN_CTRLMODE_LISTENONLY | CAN_CTRLMODE_BERR_REPORTING |
		CAN_CTRLMODE_FD | CAN_CTRLMODE_FD_NON_ISO |
		CAN_CTRLMODE_CC_LEN8_DLC;
	sl->can.bittiming_const = &gd_bittiming_const;
	sl->can.data_bittiming_const = &gd_data_bittiming_const;
	/* Configure CAN metadata */

	sl->can.do_set_bittiming = slcan_set_bittiming;
	sl->can.do_set_data_bittiming = slcan_set_data_bittiming;

	/* Configure netdev interface */
	sl->dev	= dev;
	dev->netdev_ops = &slcan_netdev_ops;
	dev->ethtool_ops = &slcan_ethtool_ops;

	/* Mark ldisc channel as alive */
	sl->tty = tty;
	sl->fd_on = false;
	tty->disc_data = sl;

	err = register_candev(dev);
	if (err) {
		free_candev(dev);
		pr_err("can't register candev\n");
		return err;
	}

	netdev_info(dev, "slcan on %s.\n", tty->name);
	/* TTY layer expects 0 on success */
	return 0;
}

/* Close down a SLCAN channel.
 * This means flushing out any pending queues, and then returning. This
 * call is serialized against other ldisc functions.
 * Once this is called, no other ldisc function of ours is entered.
 *
 * We also use this method for a hangup event.
 */
static void slcan_close(struct tty_struct *tty)
{
	struct slcan *sl = (struct slcan *)tty->disc_data;

	/* unregister_netdev() calls .ndo_stop() so we don't have to.
	 * Our .ndo_stop() also flushes the TTY write wakeup handler,
	 * so we can safely set sl->tty = NULL after this.
	 */
	unregister_candev(sl->dev);

	/* Mark channel as dead */
	spin_lock_bh(&sl->lock);
	tty->disc_data = NULL;
	sl->tty = NULL;
	spin_unlock_bh(&sl->lock);

	netdev_info(sl->dev, "slcan off %s.\n", tty->name);
	free_candev(sl->dev);
}

static struct tty_ldisc_ops slcan_ldisc = {
	.owner		= THIS_MODULE,
	.num		= N_SLCAN,
	.name		= KBUILD_MODNAME,
	.open		= slcan_open,
	.close		= slcan_close,
	.ioctl		= slcan_ioctl,
	.receive_buf	= slcan_receive_buf,
	.write_wakeup	= slcan_write_wakeup,
};

static int __init slcan_init(void)
{
	int status;

	pr_info("serial line CAN interface driver\n");

	/* Fill in our line protocol discipline, and register it */
	status = kcompat_tty_register_ldisc(&slcan_ldisc);
	if (status)
		pr_err("can't register line discipline\n");

	return status;
}

static void __exit slcan_exit(void)
{
	/* This will only be called when all channels have been closed by
	 * userspace - tty_ldisc.c takes care of the module's refcount.
	 */
	kcompat_tty_unregister_ldisc(&slcan_ldisc);
}

module_init(slcan_init);
module_exit(slcan_exit);
