/*****************************************************************************/
/*
*      
*
*      Copyright (C) 2012 Exar Corporation.
*
*      Based on Linux 2.6 Kernel's  drivers/serial/8250.c
*
*      This program is free software; you can redistribute it and/or modify
*      it under the terms of the GNU General Public License as published by
*      the Free Software Foundation; either version 2 of the License, or
*      (at your option) any later version.
*
*      This program is distributed in the hope that it will be useful,
*      but WITHOUT ANY WARRANTY; without even the implied warranty of
*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*      GNU General Public License for more details.
*
*      You should have received a copy of the GNU General Public License
*      along with this program; if not, write to the Free Software
*      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*
*	   XR16L275x driver for
*	   for			: 3.11.x 
*	   date			: apil 2015
*	   version		: 1.0
*
*	Check Release Notes for information on what has changed in the new
*	version.
*/
#include <linux/platform_device.h>
#include <../../../../arch/arm/mach-imx/hardware.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/tty_flip.h>
#include <linux/serial_reg.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/byteorder.h>
#include <asm/serial.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "linux/version.h"

#define  CCM_BASE_ADDR 0x020c4000
#define WEIM_BASE_ADDR 0x021b8000

static int gpio_sysbus_int1;
static int gpio_sysbus_int2;
static int sysbus_int1_irq, sysbus_int2_irq;

//#define ENABLE_RS485_AUTO_MODE  
//#define UART_FCTR  0x01   

#define _INLINE_ inline

#define SERIALEXAR_SHARE_IRQS 1
unsigned int share_irqs = SERIALEXAR_SHARE_IRQS;

#define UART_XR275x_NR	2

#define XR_275x_MAJOR       40
#define XR_275x_MINOR       0

#define PASS_LIMIT	256

/*
 * We default to IRQ0 for the "no irq" hack.   Some
 * machine types want others as well - they're free
 * to redefine this in their header file.
 */
#define is_real_interrupt(irq)	((irq) != 0)


struct uart_xr_port {
	struct uart_port	port;
	struct timer_list	timer;		/* "no irq" timer */
	struct list_head	list;		/* ports on this IRQ */
	unsigned int		capabilities;	/* port capabilities */
	unsigned short		rev;
	unsigned char		acr;
	unsigned char		ier;
	unsigned char		lcr;
	unsigned char		mcr_mask;	/* mask of user bits */
	unsigned char		mcr_force;	/* mask of forced bits */
	unsigned char		lsr_break_flag;

	/*
	 * We provide a per-port pm hook.
	 */
	void			(*pm)(struct uart_port *port,
				      unsigned int state, unsigned int old);
};

struct irq_info {
	spinlock_t		lock;
	struct list_head	*head;
};

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0)
struct serial_uart_config {
	char	*name;
	int	dfl_xmit_fifo_size;
	int	flags;
};
#endif

static struct irq_info irq_lists[NR_IRQS];

/*
 * Here we define the default xmit fifo size used for each type of UART.
 */
#define PORT_MAX_XR 1 
#define XR275x_TYPE 1 // the second entry that is [1] in the array
static const struct serial_uart_config uart_config[PORT_MAX_XR+1] = {
	{ "Unknown",	1,	0 },
	{ "XR275x",		64,	0 },
};

void __iomem *in_membase_prev = 0;
int in_offset_prev = -1;

static _INLINE_ unsigned int serial_in(struct uart_xr_port *up, int offset)
{
	offset <<= up->port.regshift;

	switch (up->port.iotype) {
	case SERIAL_IO_HUB6:
		outb(up->port.hub6 - 1 + offset, up->port.iobase);
		return inb(up->port.iobase + 1);

	case SERIAL_IO_MEM:
		return readb(up->port.membase + offset);

	default:
		return inb(up->port.iobase + offset);
	}
}

void __iomem *out_membase_prev = 0;
int out_offset_prev = -1;
int out_value=-1;

static _INLINE_ void
serial_out(struct uart_xr_port *up, int offset, int value)
{
	offset <<= up->port.regshift;

	switch (up->port.iotype) {
	case SERIAL_IO_HUB6:
		outb(up->port.hub6 - 1 + offset, up->port.iobase);
		outb(value, up->port.iobase + 1);
		break;

	case SERIAL_IO_MEM:
		writeb(value, up->port.membase + offset);
		break;

	default:
		outb(value, up->port.iobase + offset);
	}
}

/*
 * We used to support using pause I/O for certain machines.  We
 * haven't supported this for a while, but just in case it's badly
 * needed for certain old 386 machines, I've left these #define's
 * in....
 */
#define serial_inp(up, offset)		serial_in(up, offset)
#define serial_outp(up, offset, value)	serial_out(up, offset, value)

static void serialxr275x_stop_tx(struct uart_port *port)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;

	if (up->ier & UART_IER_THRI) {
		up->ier &= ~UART_IER_THRI;
		serial_out(up, UART_IER, up->ier);
	}
}

static void serialxr275x_start_tx(struct uart_port *port)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;

	if (!(up->ier & UART_IER_THRI)) {
		up->ier |= UART_IER_THRI;
		serial_out(up, UART_IER, up->ier);
	}
}

static void serialxr275x_stop_rx(struct uart_port *port)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;

	up->ier &= ~UART_IER_RLSI;
	up->port.read_status_mask &= ~UART_LSR_DR;
	serial_out(up, UART_IER, up->ier);
}

static void serialxr275x_enable_ms(struct uart_port *port)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;

	up->ier |= UART_IER_MSI;
	serial_out(up, UART_IER, up->ier);
}

static _INLINE_ void
receive_chars(struct uart_xr_port *up, int *status)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 9, 0)	
	struct uart_port *port = &up->port;
#else
	struct tty_struct *tty = up->port.state->port.tty;
#endif
	unsigned char ch, lsr = *status;
	int max_count = 256;
	char flag;

	do {
		ch = serial_inp(up, UART_RX);
		flag = TTY_NORMAL;
		up->port.icount.rx++;

		if (unlikely(lsr & (UART_LSR_BI | UART_LSR_PE |
				    UART_LSR_FE | UART_LSR_OE))) {
			/*
			 * For statistics only
			 */
			if (lsr & UART_LSR_BI) {
				lsr &= ~(UART_LSR_FE | UART_LSR_PE);
				up->port.icount.brk++;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
				if (uart_handle_break(&up->port))
					goto ignore_char;
			} else if (lsr & UART_LSR_PE)
				up->port.icount.parity++;
			else if (lsr & UART_LSR_FE)
				up->port.icount.frame++;
			if (lsr & UART_LSR_OE)
				up->port.icount.overrun++;

			/*
			 * Mask off conditions which should be ingored.
			 */
			lsr &= up->port.read_status_mask;

			if (lsr & UART_LSR_BI) {
				//DEBUG_INTR("handling break....");
				flag = TTY_BREAK;
			} else if (lsr & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (lsr & UART_LSR_FE)
				flag = TTY_FRAME;
		}
		if (uart_handle_sysrq_char(&up->port, ch))
			goto ignore_char;

		uart_insert_char(&up->port, lsr, UART_LSR_OE, ch, flag);

	ignore_char:
		lsr = serial_inp(up, UART_LSR);
	} while ((lsr & UART_LSR_DR) && (max_count-- > 0));
	spin_unlock(&up->port.lock);
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 9, 0)
	tty_flip_buffer_push(&port->state->port);
#else
	tty_flip_buffer_push(tty);
#endif
	spin_lock(&up->port.lock);
	*status = lsr;
}

static _INLINE_ void transmit_chars(struct uart_xr_port *up)
{
	struct circ_buf *xmit = &up->port.state->xmit;
	int count;

	if (up->port.x_char) {
		serial_outp(up, UART_TX, up->port.x_char);
		up->port.icount.tx++;
		up->port.x_char = 0;
		return;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(&up->port)) {
		serialxr275x_stop_tx(&up->port);
		return;
	}

	count = up->port.fifosize;
	do {
		serial_out(up, UART_TX, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		up->port.icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);
	
	if (uart_circ_empty(xmit))
		serialxr275x_stop_tx(&up->port);
}

static _INLINE_ void check_modem_status(struct uart_xr_port *up)
{
	int status;

	status = serial_in(up, UART_MSR);

	if ((status & UART_MSR_ANY_DELTA) == 0)
		return;

	if (status & UART_MSR_TERI)
		up->port.icount.rng++;
	if (status & UART_MSR_DDSR)
		up->port.icount.dsr++;
	if (status & UART_MSR_DDCD)
		uart_handle_dcd_change(&up->port, status & UART_MSR_DCD);
	if (status & UART_MSR_DCTS)
		uart_handle_cts_change(&up->port, status & UART_MSR_CTS);

	wake_up_interruptible(&up->port.state->port.delta_msr_wait);
}

/*
 * This handles the interrupt from one port.
 */
static inline void
serialxr275x_handle_port(struct uart_xr_port *up)
{
	unsigned int status = serial_inp(up, UART_LSR);

	if (status & UART_LSR_DR)
		receive_chars(up, &status);
	check_modem_status(up);
	if (status & UART_LSR_THRE)
		transmit_chars(up);
}

/*
 * This is the serial driver's interrupt routine.
 *
 * Arjan thinks the old way was overly complex, so it got simplified.
 * Alan disagrees, saying that need the complexity to handle the weird
 * nature of ISA shared interrupts.  (This is a special exception.)
 *
 * In order to handle ISA shared interrupts properly, we need to check
 * that all ports have been serviced, and therefore the ISA interrupt
 * line has been de-asserted.
 *
 * This means we need to loop through all ports. checking that they
 * don't have an interrupt pending.
 */
static irqreturn_t serialxr275x_interrupt(int irq, void *dev_id)
{
	struct irq_info *i = dev_id;
	struct list_head *l, *end = NULL;
	int pass_counter = 0;

	spin_lock(&i->lock);

	l = i->head;
	do {
		struct uart_xr_port *up;
		unsigned int iir;
		
		up = list_entry(l, struct uart_xr_port, list);
		iir = serial_in(up, UART_IIR);

		if (!(iir & UART_IIR_NO_INT)) {
			spin_lock(&up->port.lock);
			serialxr275x_handle_port(up);
			spin_unlock(&up->port.lock);

			end = NULL;
		} else if (end == NULL)
			end = l;

		l = l->next;

		if (l == i->head && pass_counter++ > PASS_LIMIT) {
			/* If we hit this, we're dead. */
			printk(KERN_ERR "serialxr275x: too much work for "
				"irq%d\n", irq);
			break;
		}
	} while (l != end);

	spin_unlock(&i->lock);

	//DEBUG_INTR("end.\n");
	/* FIXME! Was it really ours? */
	return IRQ_HANDLED;
}

/*
 * To support ISA shared interrupts, we need to have one interrupt
 * handler that ensures that the IRQ line has been deasserted
 * before returning.  Failing to do this will result in the IRQ
 * line being stuck active, and, since ISA irqs are edge triggered,
 * no more IRQs will be seen.
 */
static void serial_do_unlink(struct irq_info *i, struct uart_xr_port *up)
{
	spin_lock_irq(&i->lock);

	if (!list_empty(i->head)) {
		if (i->head == &up->list)
			i->head = i->head->next;
		list_del(&up->list);
	} else {
		BUG_ON(i->head != &up->list);
		i->head = NULL;
	}

	spin_unlock_irq(&i->lock);
}

static void set_serial_irq_type(int irq) {	
	if(irq == sysbus_int1_irq)
		irq_set_irq_type(sysbus_int1_irq, IRQ_TYPE_LEVEL_HIGH);
	else if(irq == sysbus_int2_irq)
		irq_set_irq_type(sysbus_int2_irq, IRQ_TYPE_LEVEL_HIGH);
	else
		printk("\n[set_serial_irq_type] Error irq:%d\n", irq);
}

static int serial_link_irq_chain(struct uart_xr_port *up)
{
	struct irq_info *i = irq_lists + up->port.irq;
	int ret, irq_flags = up->port.flags & UPF_SHARE_IRQ ? IRQF_SHARED : 0;

	spin_lock_irq(&i->lock);

	if (i->head) {
		list_add(&up->list, i->head);
		spin_unlock_irq(&i->lock);

		ret = 0;
	} else {
		INIT_LIST_HEAD(&up->list);
		i->head = &up->list;
		spin_unlock_irq(&i->lock);

#if 0 //alex
		ret = request_irq(up->port.irq, serialxr275x_interrupt,
				  irq_flags, "xrserial", i);
#else
		set_serial_irq_type(up->port.irq);
		ret = request_irq(up->port.irq, serialxr275x_interrupt,
				  irq_flags|IRQF_TRIGGER_HIGH, "xrserial", i);
#endif

		if (ret < 0)
			serial_do_unlink(i, up);
	}

	return ret;
}

static void serial_unlink_irq_chain(struct uart_xr_port *up)
{
	struct irq_info *i = irq_lists + up->port.irq;

	BUG_ON(i->head == NULL);

	if (list_empty(i->head))
		free_irq(up->port.irq, i);

	serial_do_unlink(i, up);
}

/*
 * This function is used to handle ports that do not have an
 * interrupt.  This doesn't work very well for 16450's, but gives
 * barely passable results for a 16550A.  (Although at the expense
 * of much CPU overhead).
 */
static void serialxr275x_timeout(unsigned long data)
{
	struct uart_xr_port *up = (struct uart_xr_port *)data;
	unsigned int timeout;
	unsigned int iir;
	
	iir = serial_in(up, UART_IIR);
	if (!(iir & UART_IIR_NO_INT)) {
		spin_lock(&up->port.lock);
		serialxr275x_handle_port(up);
		spin_unlock(&up->port.lock);
	}

	timeout = up->port.timeout;
	timeout = timeout > 6 ? (timeout / 2 - 2) : 1;
	mod_timer(&up->timer, jiffies + timeout);
}

static unsigned int serialxr275x_tx_empty(struct uart_port *port)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;
	unsigned long flags;
	unsigned int ret;

	spin_lock_irqsave(&up->port.lock, flags);
	ret = serial_in(up, UART_LSR) & UART_LSR_TEMT ? TIOCSER_TEMT : 0;
	spin_unlock_irqrestore(&up->port.lock, flags);

	return ret;
}

static unsigned int serialxr275x_get_mctrl(struct uart_port *port)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;
	unsigned long flags;
	unsigned char status;
	unsigned int ret;

	spin_lock_irqsave(&up->port.lock, flags);
	status = serial_in(up, UART_MSR);
	spin_unlock_irqrestore(&up->port.lock, flags);

	ret = 0;
	if (status & UART_MSR_DCD)
		ret |= TIOCM_CAR;
	if (status & UART_MSR_RI)
		ret |= TIOCM_RNG;
	if (status & UART_MSR_DSR)
		ret |= TIOCM_DSR;
	if (status & UART_MSR_CTS)
		ret |= TIOCM_CTS;
	return ret;
}

static void serialxr275x_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;
	unsigned char mcr = 0;

	if (mctrl & TIOCM_RTS)
		mcr |= UART_MCR_RTS;
	if (mctrl & TIOCM_DTR)
		mcr |= UART_MCR_DTR;
	if (mctrl & TIOCM_OUT1)
		mcr |= UART_MCR_OUT1;
	if (mctrl & TIOCM_OUT2)
		mcr |= UART_MCR_OUT2;
	if (mctrl & TIOCM_LOOP)
		mcr |= UART_MCR_LOOP;

	mcr = (mcr & up->mcr_mask) | up->mcr_force;

	serial_out(up, UART_MCR, mcr);
}

static void serialxr275x_break_ctl(struct uart_port *port, int break_state)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;
	unsigned long flags;

	spin_lock_irqsave(&up->port.lock, flags);
	if (break_state == -1)
		up->lcr |= UART_LCR_SBC;
	else
		up->lcr &= ~UART_LCR_SBC;
	serial_out(up, UART_LCR, up->lcr);
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static int serialxr275x_startup(struct uart_port *port)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;
	unsigned long flags;
	int retval;

#ifdef ENABLE_RS485_AUTO_MODE
    unsigned char tmp;
#endif

	up->capabilities = uart_config[up->port.type].flags;

	/*
	 * Clear the FIFO buffers and disable them.
	 * (they will be reeanbled in set_termios())
	 */
	serial_outp(up, UART_FCR, UART_FCR_ENABLE_FIFO |
			UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);

	serial_outp(up, UART_FCR, 0);

	/*
	 * Clear the interrupt registers.
	 */
	(void) serial_inp(up, UART_LSR);
	(void) serial_inp(up, UART_RX);
	(void) serial_inp(up, UART_IIR);
	(void) serial_inp(up, UART_MSR);

	/*
	 * If the "interrupt" for this port doesn't correspond with any
	 * hardware interrupt, we use a timer-based system.  The original
	 * driver used to do this with IRQ0.
	 */
	if (!is_real_interrupt(up->port.irq)) {
		unsigned int timeout = up->port.timeout;

		timeout = timeout > 6 ? (timeout / 2 - 2) : 1;

		up->timer.data = (unsigned long)up;
		mod_timer(&up->timer, jiffies + timeout);
	} else {
		retval = serial_link_irq_chain(up);
		if (retval)
			return retval;
	}
	
#ifdef ENABLE_RS485_AUTO_MODE
    serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_B);
	tmp = serial_in(up, UART_FCTR);
	tmp |= (1<<3);//enable Auto RS485 Half-Duplex
	serial_outp(up, UART_FCTR, tmp);
#endif	
	/*
	 * Now, initialize the UART
	 */
	serial_outp(up, UART_LCR, UART_LCR_WLEN8);

	spin_lock_irqsave(&up->port.lock, flags);
		
	/*
	* Most PC uarts need OUT2 raised to enable interrupts.
	*/
	if (is_real_interrupt(up->port.irq))
		up->port.mctrl |= TIOCM_OUT2;

	serialxr275x_set_mctrl(&up->port, up->port.mctrl);
	spin_unlock_irqrestore(&up->port.lock, flags);

	/*
	 * Finally, enable interrupts.  Note: Modem status interrupts
	 * are set via set_termios(), which will be occurring imminently
	 * anyway, so we don't enable them here.
	 */
	up->ier = UART_IER_MSI | UART_IER_RLSI | UART_IER_RDI;	
	serial_outp(up, UART_IER, up->ier);

	/*
	 * And clear the interrupt registers again for luck.
	 */
	(void) serial_inp(up, UART_LSR);
	(void) serial_inp(up, UART_RX);
	(void) serial_inp(up, UART_IIR);
	(void) serial_inp(up, UART_MSR);

	return 0;
}

static void serialxr275x_shutdown(struct uart_port *port)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;
	unsigned long flags;
	
	/*
	 * Disable interrupts from this port
	 */
	up->ier = 0;
	serial_outp(up, UART_IER, 0);

	spin_lock_irqsave(&up->port.lock, flags);
	
	up->port.mctrl &= ~TIOCM_OUT2;

	serialxr275x_set_mctrl(&up->port, up->port.mctrl);
	spin_unlock_irqrestore(&up->port.lock, flags);

	/*
	 * Disable break condition and FIFOs
	 */
	serial_out(up, UART_LCR, serial_inp(up, UART_LCR) & ~UART_LCR_SBC);
	serial_outp(up, UART_FCR, UART_FCR_ENABLE_FIFO |
				  UART_FCR_CLEAR_RCVR |
				  UART_FCR_CLEAR_XMIT);
	serial_outp(up, UART_FCR, 0);

	/*
	 * Read data port to reset things, and then unlink from
	 * the IRQ chain.
	 */
	(void) serial_in(up, UART_RX);

	if (!is_real_interrupt(up->port.irq))
		del_timer_sync(&up->timer);
	else
		serial_unlink_irq_chain(up);
}

static unsigned int serialxr275x_get_divisor(struct uart_port *port, unsigned int baud)
{
	unsigned int quot;

	quot = uart_get_divisor(port, baud);

	return quot;
}

static void
serialxr275x_set_termios(struct uart_port *port, struct ktermios *termios,
		       struct ktermios *old)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;
	unsigned char cval;
	unsigned long flags;
	unsigned int baud, quot, quot_fraction;

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		cval = 0x00;
		break;
	case CS6:
		cval = 0x01;
		break;
	case CS7:
		cval = 0x02;
		break;
	default:
	case CS8:
		cval = 0x03;
		break;
	}

	if (termios->c_cflag & CSTOPB)
		cval |= 0x04;
	if (termios->c_cflag & PARENB)
		cval |= UART_LCR_PARITY;
	if (!(termios->c_cflag & PARODD))
		cval |= UART_LCR_EPAR;
#ifdef CMSPAR
	if (termios->c_cflag & CMSPAR)
		cval |= UART_LCR_SPAR;
#endif

	/*
	 * Ask the core to calculate the divisor for us.
	 */
	baud = uart_get_baud_rate(port, termios, old, 0, port->uartclk/16); 
	quot = serialxr275x_get_divisor(port, baud);
	quot_fraction = ( (port->uartclk/baud) - (16*quot));
	
	/*
	 * Ok, we're now changing the port state.  Do it with
	 * interrupts disabled.
	 */
	spin_lock_irqsave(&up->port.lock, flags);

	/*
	 * Update the per-port timeout.
	 */
	uart_update_timeout(port, termios->c_cflag, baud);

	up->port.read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (termios->c_iflag & INPCK)
		up->port.read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (BRKINT | PARMRK))
		up->port.read_status_mask |= UART_LSR_BI;

	/*
	 * Characteres to ignore
	 */
	up->port.ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		up->port.ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		up->port.ignore_status_mask |= UART_LSR_BI;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			up->port.ignore_status_mask |= UART_LSR_OE;
	}

	/*
	 * ignore all characters if CREAD is not set
	 */
	if ((termios->c_cflag & CREAD) == 0)
		up->port.ignore_status_mask |= UART_LSR_DR;

	/*
	 * CTS flow control flag and modem status interrupts
	 */
	up->ier &= ~UART_IER_MSI;
	if (UART_ENABLE_MS(&up->port, termios->c_cflag))
		up->ier |= UART_IER_MSI;

	serial_out(up, UART_IER, up->ier);

	serial_outp(up, UART_LCR, cval | UART_LCR_DLAB);/* set DLAB */
	
	serial_outp(up, UART_DLL, quot & 0xff);		/* LS of divisor */
	serial_outp(up, UART_DLM, quot >> 8);		/* MS of divisor */

	serial_outp(up, UART_LCR, cval);		/* reset DLAB */
	up->lcr = cval;					/* Save LCR */
	
	serial_outp(up, UART_FCR, UART_FCR_ENABLE_FIFO);/* set fcr */
	
	spin_unlock_irqrestore(&up->port.lock, flags);
}

/*
 *      EXAR ioctls
 */
//#define 	FIOQSIZE		0x5460 
#define		EXAR_READ_REG      	(FIOQSIZE + 1)
#define 	EXAR_WRITE_REG     	(FIOQSIZE + 2)

struct xrioctl_rw_reg {
	unsigned char reg;
	unsigned char regvalue;
};
/*
 * This function is used to handle Exar Device specific ioctl calls
 * The user level application should have defined the above ioctl
 * commands with the above values to access these ioctls and the 
 * input parameters for these ioctls should be struct xrioctl_rw_reg
 * The Ioctl functioning is pretty much self explanatory here in the code,
 * and the register values should be standard UART offsets.
 */

static int
serialxr275x_ioctl(struct uart_port *port, unsigned int cmd, unsigned long arg)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;
	int ret = -ENOIOCTLCMD;
	struct xrioctl_rw_reg ioctlrwarg;

	switch (cmd)
	{
		case EXAR_READ_REG:
		if (copy_from_user(&ioctlrwarg, (void *)arg, sizeof(ioctlrwarg)))
			return -EFAULT;
		ioctlrwarg.regvalue = serial_inp(up, ioctlrwarg.reg);
		if (copy_to_user((void *)arg, &ioctlrwarg, sizeof(ioctlrwarg)))
			return -EFAULT;
		ret = 0;
		break;
		
		case EXAR_WRITE_REG:
		if (copy_from_user(&ioctlrwarg, (void *)arg, sizeof(ioctlrwarg)))
			return -EFAULT;
		serial_outp(up, ioctlrwarg.reg, ioctlrwarg.regvalue);
		ret = 0;
		break;
	}
	
	return ret;
}
	      
static void
serialxr275x_pm(struct uart_port *port, unsigned int state,
	      unsigned int oldstate)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;
	if (state) {
		/* sleep */
		serial_outp(up, UART_IER, UART_IERX_SLEEP);
						
		if (up->pm)
			up->pm(port, state, oldstate);
	} else {
		/* wake */
		
		/* Wake up UART */
		serial_outp(up, UART_IER, 0);
		
		if (up->pm)
			up->pm(port, state, oldstate);
	}
}

static void serialxr275x_release_port(struct uart_port *port)
{	
}

static int serialxr275x_request_port(struct uart_port *port)
{
	return 0;
}

static void serialxr275x_config_port(struct uart_port *port, int flags)
{
	struct uart_xr_port *up = (struct uart_xr_port *)port;
	
	if (flags & UART_CONFIG_TYPE)
	{
		up->port.type = XR275x_TYPE;
		up->port.fifosize = uart_config[up->port.type].dfl_xmit_fifo_size;
		up->capabilities = uart_config[up->port.type].flags;
	}
}

static const char *
serialxr275x_type(struct uart_port *port)
{
	int type = port->type;
	
	if (type >= ARRAY_SIZE(uart_config))
		type = 0;
	return uart_config[type].name;
}

static struct uart_ops serialxr275x_pops = {
	.tx_empty	= serialxr275x_tx_empty,
	.set_mctrl	= serialxr275x_set_mctrl,
	.get_mctrl	= serialxr275x_get_mctrl,
	.stop_tx	= serialxr275x_stop_tx,
	.start_tx	= serialxr275x_start_tx,
	.stop_rx	= serialxr275x_stop_rx,
	.enable_ms	= serialxr275x_enable_ms,
	.break_ctl	= serialxr275x_break_ctl,
	.startup	= serialxr275x_startup,
	.shutdown	= serialxr275x_shutdown,
	.set_termios	= serialxr275x_set_termios,
	.pm		= serialxr275x_pm,
	.type		= serialxr275x_type,
	.release_port	= serialxr275x_release_port,
	.request_port	= serialxr275x_request_port,
	.config_port	= serialxr275x_config_port,
	.ioctl		= serialxr275x_ioctl,
};

static struct uart_xr_port serialxr275x_ports[UART_XR275x_NR];

#define XR275x_BASE1		0x08000000
#define XR275x_BASE2		0x0C000000
#define XR275x_UART_OFFSET 	0x4000000 //64 MB

static int Get_IRQ_Num(int num)
{
	if(num == 0)
	{
		sysbus_int1_irq = gpio_to_irq(gpio_sysbus_int1);

		if (sysbus_int1_irq < 0)
		{
			printk("\n[Get_IRQ_Num] Get error sysbus_int1_irq\n");
			return 0;
		}

		return sysbus_int1_irq;
	}
	else if(num == 1)
	{	
		sysbus_int2_irq = gpio_to_irq(gpio_sysbus_int2);

		if (sysbus_int1_irq < 0)
		{
			printk("\n[Get_IRQ_Num] Get error sysbus_int2_irq\n");
			return 0;
		}

		return sysbus_int2_irq;
	}
	else
	{
		printk("\n[Get_IRQ_Num] Error Num:%d\n", num);
		return 0;
	}
}

static void __init serial275x_init_ports(void)
{
	struct uart_xr_port *up;
	static int first = 1;
	int i;
	
	if (!first)
		return;
	first = 0;

	for (i = 0, up = serialxr275x_ports; i < ARRAY_SIZE(serialxr275x_ports);
	     i++, up++) {
		up->port.iobase   = 0; 

		if(i == 0)
			up->port.mapbase = 	XR275x_BASE1;
		else if (i == 1)
			up->port.mapbase = 	XR275x_BASE2;

		up->port.irq      = Get_IRQ_Num(i);
		up->port.uartclk  = 921600 * 16; // 14.7 MHz clock rate
		up->port.flags    = UPF_SKIP_TEST | UPF_BOOT_AUTOCONF | UPF_SHARE_IRQ;
		up->port.hub6     = 0;
		up->port.membase  = ioremap(XR275x_BASE1 + (i*XR275x_UART_OFFSET), XR275x_UART_OFFSET);  // memory mapped
		up->port.iotype   = SERIAL_IO_MEM; 
		up->port.regshift = 2; // Address Data Line Shift - SYSBUS_A2 start
		up->port.ops      = &serialxr275x_pops;
		
		if (share_irqs)
			up->port.flags |= UPF_SHARE_IRQ;
	}
//for (i=0; i<8; i++)  
//   printk(KERN_INFO "xr16m275x.c: membase UART reg offset %d=0x%x\n",i,readb((port->membase) + i));
}

static void __init serialxr275x_register_ports(struct uart_driver *drv)
{
	int i;
	
	serial275x_init_ports();
	
	for (i = 0; i < UART_XR275x_NR; i++) {
		struct uart_xr_port *up = &serialxr275x_ports[i];

		up->port.line = i;
		up->port.ops = &serialxr275x_pops;
		init_timer(&up->timer);
		up->timer.function = serialxr275x_timeout;
		
		/*
		 * ALPHA_KLUDGE_MCR needs to be killed.
		 */
		up->mcr_mask = ~(0x0); //~ALPHA_KLUDGE_MCR;
		up->mcr_force = 0; // ALPHA_KLUDGE_MCR;
		uart_add_one_port(drv, &up->port);
	}
}

#define SERIALXR_CONSOLE	NULL

static struct uart_driver serialxr275x_reg = {
	.owner			= THIS_MODULE,
	.driver_name		= "xrserial",
	.dev_name		= "ttyS",
	.major			= XR_275x_MAJOR,
	.minor			= XR_275x_MINOR,
	.nr			= UART_XR275x_NR,
	.cons			= SERIALXR_CONSOLE,
};

static int serialxr275x_init(void)
{
	int ret, i;

	printk(KERN_INFO "XR16M275x specific serial driver $Revision: 1.0 $ "
		"%d ports, IRQ sharing %sabled\n", (int) UART_XR275x_NR,
		share_irqs ? "en" : "dis");

	for (i = 0; i < NR_IRQS; i++)
		spin_lock_init(&irq_lists[i].lock);

	ret = uart_register_driver(&serialxr275x_reg);
	
	if (ret >= 0)
		serialxr275x_register_ports(&serialxr275x_reg);
		
	return ret;
}

static void serialxr275x_exit(void)
{
	int i;
			
	for (i = 0; i < UART_XR275x_NR; i++)
		uart_remove_one_port(&serialxr275x_reg, &serialxr275x_ports[i].port);
	
	uart_unregister_driver(&serialxr275x_reg);
}

/* Config CS0 and CS1 settings */
static void weim_cs_config(void)
{
	u32 reg;
	void __iomem *weim_base, *iomuxc_base;

	weim_base = ioremap(WEIM_BASE_ADDR, SZ_4K);

	/* CS0 */
	writel(0x7E20001, (weim_base)); //8 bit port resides on DATA[23:16]
	writel(0x0, (weim_base + 0x4));
	writel(0x16004422, (weim_base + 0x8));
	writel(0x00000002, (weim_base + 0xC));
	writel(0x16002982, (weim_base + 0x10));
	writel(0x00000000, (weim_base + 0x14));
	writel(0x00000000, (weim_base + 0x90)); // interrupt high or low active - at present - low active

	/* CS1 */
	writel(0x7E20001, (weim_base + 0x18));
	writel(0x0, (weim_base + 0x1C));
	writel(0x16004422, (weim_base + 0x20));
	writel(0x00000002, (weim_base + 0x24));
	writel(0x16002982, (weim_base + 0x28));
	writel(0x00000000, (weim_base + 0x2C));
	writel(0x00000000, (weim_base + 0x90)); // interrupt high or low active - at present - low active

	/* specify 64 MB on CS1 and CS0 on GPR1 */
	iomuxc_base = ioremap(MX6Q_IOMUXC_BASE_ADDR, SZ_4K);
	reg = readl(iomuxc_base + 0x4);
	reg &= ~0x3F;
	reg |= 0x1B;

	writel(reg, (iomuxc_base + 0x4));

	iounmap(iomuxc_base);
	iounmap(weim_base);
}

static int adv_init_extuart(void)
{
	return serialxr275x_init();
}

static int adv_sysbus_uart_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int err;

	gpio_sysbus_int1 = of_get_named_gpio(np, "sysbus-int1", 0);

	if (!gpio_is_valid(gpio_sysbus_int1))
	{
		printk("\n[adv_sysbus_uart_probe] No gpio_sysbus_int1");
		return -ENODEV;	
	}

	err = devm_gpio_request(&pdev->dev, gpio_sysbus_int1, "GPIO sysbus-int1");

	if (err)
	{
		printk("\n[adv_sysbus_uart_probe] Request gpio_sysbus_int1 failed");
		return err;
	}

	gpio_direction_input(gpio_sysbus_int1);

	gpio_sysbus_int2 = of_get_named_gpio(np, "sysbus-int2", 0);

	if (!gpio_is_valid(gpio_sysbus_int2))
		return -ENODEV;	

	err = devm_gpio_request(&pdev->dev, gpio_sysbus_int2, "GPIO sysbus-int2");

	if (err)
	{
		printk("\n[adv_sysbus_uart_probe] Request gpio_sysbus_int2 failed");
		return err;
	}

	gpio_direction_input(gpio_sysbus_int2);

	weim_cs_config();

	adv_init_extuart();
	
	return 0;
}

static int adv_sysbus_uart_remove(struct platform_device *pdev)
{
	serialxr275x_exit();

	return 0;
}

static const struct of_device_id adv_sysbus_uart_dt_ids[] = {
	{ .compatible = "adv-sysbus-uart", },
	{},
};

static struct platform_driver adv_sysbus_uart_driver = {
	.probe = adv_sysbus_uart_probe,
	.remove	= adv_sysbus_uart_remove,
	.driver = {
		.name = "adv-sysbus-uart",
		.owner = THIS_MODULE,
		.of_match_table = adv_sysbus_uart_dt_ids,
	},
};

static int __init adv_sysbus_uart_init(void)
{
	return platform_driver_register(&adv_sysbus_uart_driver);
}

static void __exit adv_sysbus_uart_exit(void)
{
	platform_driver_unregister(&adv_sysbus_uart_driver);
}

module_init(adv_sysbus_uart_init);
module_exit(adv_sysbus_uart_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("XR16M275x specific serial driver $Revision: 1.0 $");
MODULE_ALIAS("platform:Advantech System Bus Uart Driver");

