/* SPDX-License-Identifier: GPL-2.0
 * slcan.h - serial line CAN interface driver
 *
 * Copyright (C) Laurence Culhane <loz@holmes.demon.co.uk>
 * Copyright (C) Fred N. van Kempen <waltje@uwalt.nl.mugnet.org>
 * Copyright (C) Oliver Hartkopp <socketcan@hartkopp.net>
 * Copyright (C) 2022 Amarula Solutions, Dario Binacchi <dario.binacchi@amarulasolutions.com>
 *
 */

#ifndef _SLCAN_H
#define _SLCAN_H

/* maximum rx buffer len: extended CAN frame with timestamp */
/* support CANFD MTU */
#define SLCAN_MTU (sizeof("b11112222F"      \
                                                "1122334455667788" \
                                                "1122334455667788" \
                                                "1122334455667788" \
                                                "1122334455667788" \
                                                "1122334455667788" \
                                                "1122334455667788" \
                                                "1122334455667788" \
                                                "1122334455667788EA5F\r") + 1)

struct slcan {
	struct can_priv         can;

	/* Various fields. */
	struct tty_struct	*tty;		/* ptr to TTY structure	     */
	struct net_device	*dev;		/* easy for intr handling    */
	spinlock_t		lock;
	struct work_struct	tx_work;	/* Flushes transmit buffer   */

	/* These are pointers to the malloc()ed frame buffers. */
	unsigned char		rbuff[SLCAN_MTU];	/* receiver buffer   */
	int			rcount;         /* received chars counter    */
	unsigned char		xbuff[SLCAN_MTU];	/* transmitter buffer*/
	unsigned char		*xhead;         /* pointer to next XMIT byte */
	int			xleft;          /* bytes left in XMIT queue  */

	unsigned long		flags;		/* Flag values/ mode etc     */
#define SLF_ERROR		0               /* Parity, etc. error        */
#define SLF_XCMD		1               /* Command transmission      */
	unsigned long           cmd_flags;      /* Command flags             */
#define CF_ERR_RST		0               /* Reset errors on open      */
	wait_queue_head_t       xcmd_wait;      /* Wait queue for commands   */
						/* transmission              */

	bool			fd_on;
};

bool slcan_err_rst_on_open(struct net_device *ndev);
int slcan_enable_err_rst_on_open(struct net_device *ndev, bool on);

extern const struct ethtool_ops slcan_ethtool_ops;

#endif /* _SLCAN_H */
