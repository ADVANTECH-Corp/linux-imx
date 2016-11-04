#ifdef CONFIG_ARCH_ADVANTECH
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

extern char board_type[20];

static int boardinfo_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s", board_type);

	return 0;
}

static int boardinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, boardinfo_proc_show, NULL);
}

static const struct file_operations boardinfo_proc_fops = {
	.open		= boardinfo_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_boardinfo_init(void)
{
	proc_create("board", 0, NULL, &boardinfo_proc_fops);
	return 0;
}
module_init(proc_boardinfo_init);
#endif
