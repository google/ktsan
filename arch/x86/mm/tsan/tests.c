#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/string.h>

#include <asm/uaccess.h>
#include <asm/thread_info.h>

static int current_thread_id(void)
{
	return current_thread_info()->task->pid;
}

static void tsan_run_tests(void)
{
	printk("TSan: running tests, thread #%d.\n", current_thread_id());
}

static ssize_t tsan_tests_write(struct file *file, const char __user *buf,
				size_t count, loff_t *offset)
{
	char buffer[16];

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	if (!strcmp(buffer, "tsan_run_tests\n"))
		tsan_run_tests();

	return count;
}

static const struct file_operations tsan_tests_operations = {
	.write = tsan_tests_write,
};

static int __init tsan_tests_init(void)
{
	proc_create("ktsan_tests", S_IWUSR, NULL, &tsan_tests_operations);
	return 0;
}

device_initcall(tsan_tests_init);
