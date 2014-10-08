#include "ktsan.h"

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>

static ssize_t kt_tests_write(struct file *file, const char __user *buf,
			      size_t count, loff_t *offset)
{
	char buffer[16];

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	if (!strcmp(buffer, "tsan_run_tests\n"))
		kt_tests_run();

	return count;
}

static const struct file_operations kt_tests_operations = {
	.write = kt_tests_write,
};

void kt_tests_init(void)
{
	proc_create("ktsan_tests", S_IWUSR, NULL, &kt_tests_operations);
}
