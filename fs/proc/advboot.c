#ifdef CONFIG_ARCH_ADVANTECH

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

extern char board_advboot_version[];

static int advboot_version_proc_show(struct seq_file *m, void *v)
{
	char uboot_temp_str[20] = "uboot_version=";
	char advboot_temp_str[20] = "advboot_version=";
	char *uboot_ver, *advboot_ver;
	int uboot_len=0, total_len=0, advboot_len=0; 

	/* uboot */
	uboot_ver = strstr(saved_command_line, uboot_temp_str);
	if (uboot_ver) {
		uboot_len = strlen(uboot_ver);
		uboot_ver = uboot_ver + strlen(uboot_temp_str);
	}

	/* advboot */
	advboot_ver = strstr(saved_command_line, advboot_temp_str);
	if (advboot_ver) {
		advboot_ver = advboot_ver + strlen(advboot_temp_str);
		total_len = strlen(advboot_ver);
		advboot_len = total_len - uboot_len - 1;
		strncpy(board_advboot_version, advboot_ver, advboot_len);
	}
	seq_printf(m, "%s\n", board_advboot_version);

	return 0;
}

static int advboot_version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, advboot_version_proc_show, NULL);
}

static const struct file_operations advboot_version_proc_fops = {
	.open		= advboot_version_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_advboot_version_init(void)
{
	proc_create("advboot_version", 0, NULL, &advboot_version_proc_fops);
	return 0;
}
fs_initcall(proc_advboot_version_init);

#endif
