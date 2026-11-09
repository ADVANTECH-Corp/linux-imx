#ifdef CONFIG_ARCH_ADVANTECH

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

extern char board_type[20];
static int boardinfo_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, board_type);
	seq_putc(m, '\n');

	return 0;
}

static int __init proc_boardinfo_init(void)
{
	proc_create_single("board", 0, NULL, boardinfo_proc_show);
	return 0;
}
fs_initcall(proc_boardinfo_init);

#endif
