#include "kcompat.h"
#include "slcan.h"

/* Perform I/O control on an active SLCAN channel. */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(5,16,0) )
int slcan_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd,
 		       unsigned long arg)
#else
int slcan_ioctl(struct tty_struct *tty, unsigned int cmd,
		       unsigned long arg)
#endif
{
	struct slcan *sl = (struct slcan *)tty->disc_data;
	unsigned int tmp;

	switch (cmd) {
	case SIOCGIFNAME:
		tmp = strlen(sl->dev->name) + 1;
		if (copy_to_user((void __user *)arg, sl->dev->name, tmp))
			return -EFAULT;
		return 0;

	case SIOCSIFHWADDR:
		return -EINVAL;

	default:
	#if ( LINUX_VERSION_CODE < KERNEL_VERSION(5,16,0) )
		return tty_mode_ioctl(tty, file, cmd, arg);
	#else
		return tty_mode_ioctl(tty, cmd, arg);
	#endif
	}
}
