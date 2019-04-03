#ifdef CONFIG_ARCH_ADVANTECH

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

extern char board_uboot_version[];

static int uboot_version_proc_show(struct seq_file *m, void *v)
{
	char uboot_temp_str[20] = "uboot_version=";
	char *uboot_ver;
	int uboot_len=0;

    uboot_ver = strstr(saved_command_line, uboot_temp_str);
    if (uboot_ver) {
    	uboot_len = strlen(uboot_ver);
    	uboot_ver = uboot_ver + strlen(uboot_temp_str);
    	strcpy(board_uboot_version, uboot_ver);
    }
	seq_printf(m, "%s\n", board_uboot_version);

	return 0;
}

static int uboot_version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, uboot_version_proc_show, NULL);
}

static const struct file_operations uboot_version_proc_fops = {
	.open		= uboot_version_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_uboot_version_init(void)
{
	proc_create("uboot_version", 0, NULL, &uboot_version_proc_fops);
	return 0;
}
fs_initcall(proc_uboot_version_init);

#endif
