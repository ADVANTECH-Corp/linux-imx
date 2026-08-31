#ifndef _KCOMPAT_H_
#define _KCOMPAT_H_

#include <linux/version.h>

#include <linux/module.h>

#include <linux/uaccess.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/skb.h>


#ifndef CAN_ERR_CNT
#define CAN_ERR_CNT          0x00000200U /* TX error counter / data[6] */
                                         /* RX error counter / data[7] */
#endif

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(6,0,0) )
#define CAN_BITRATE_UNSET 0
#define CAN_BITRATE_UNKNOWN (-1U)
#endif

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0) )
#define kcompat_tty_register_ldisc(ldisc) tty_register_ldisc(N_SLCAN, ldisc)
#define kcompat_tty_unregister_ldisc(ldisc) tty_unregister_ldisc(N_SLCAN)
#else
#define kcompat_tty_register_ldisc(ldisc) tty_register_ldisc(ldisc)
#define kcompat_tty_unregister_ldisc(ldisc) tty_unregister_ldisc(ldisc)
#endif

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(5,11,0) )
#define CAN_CTRLMODE_CC_LEN8_DLC	0x100	/* Classic CAN DLC option */
#endif

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(5,16,0) )
#define TTY_MODE_IOCTL(tty, file, cmd, arg) tty_mode_ioctl(tty, file, cmd, arg);
#else
#define TTY_MODE_IOCTL(tty, file, cmd, arg) tty_mode_ioctl(tty, cmd, arg);
#endif

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(5,16,0) )
int slcan_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg);
#else
int slcan_ioctl(struct tty_struct *tty, unsigned int cmd, unsigned long arg);
#endif

#if __has_attribute(__fallthrough__)
# define fallthrough                    __attribute__((__fallthrough__))
#else
# define fallthrough                    do {} while (0)  /* fallthrough */
#endif

#endif
