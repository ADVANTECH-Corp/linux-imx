#ifdef CONFIG_ARCH_ADVANTECH

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

extern char board_type[20];

static int boardinfo_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", board_type);

	return 0;
}

static int boardinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, boardinfo_proc_show, NULL);
}

static const struct proc_ops boardinfo_proc_fops = {
	.proc_open		= boardinfo_proc_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release,
};

static int __init proc_boardinfo_init(void)
{
	proc_create("board", 0, NULL, &boardinfo_proc_fops);
	return 0;
}
fs_initcall(proc_boardinfo_init);

#endif
