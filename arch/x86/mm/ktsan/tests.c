#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <asm/uaccess.h>
#include <asm/thread_info.h>

/* KTsan test: race. */

static int race_thread_first(void *arg)
{
	int *value = (int *)arg;

	do {
		ktsan_access_memory((unsigned long)arg + 1, 2, true);
		schedule();
	} while (*value == 0);
	ktsan_access_memory((unsigned long)arg + 1, 2, false);
	*value = 0;

	return *value;
}

static int race_thread_second(void *arg)
{
	int *value = (int *)arg;

	ktsan_access_memory((unsigned long)arg, 4, false);
	*value = 1;

	return *value;
}

static void ktsan_test_race(void)
{
	struct task_struct *thread_first, *thread_second;

	char thread_name_first[] = "thread-race-first";
	char thread_name_second[] = "thread-race-second";

	int* value = kmalloc(sizeof(int), GFP_KERNEL);
	BUG_ON(!value);

	pr_err("TSan: starting test, race expected.\n");

	thread_first = kthread_create(race_thread_first, value, thread_name_first);
	thread_second = kthread_create(race_thread_second, value, thread_name_second);

	if (!thread_first || !thread_second) {
		pr_err("TSan: could not create kernel threads.\n");
		return;
	}

	wake_up_process(thread_first);
	wake_up_process(thread_second);

	msleep(100);

	kthread_stop(thread_first);
	kthread_stop(thread_second);

	kfree(value);

	pr_err("TSan: end of test.\n");
}

/* KTSan test: no race. */

/*
static void ktsan_test_no_race(void)
{

}
*/

/* Other testing routines. */

static int current_thread_id(void)
{
	return current_thread_info()->task->pid;
}

static void ktsan_run_tests(void)
{
	pr_err("TSan: running tests, thread #%d.\n", current_thread_id());
	ktsan_test_race();
}

static ssize_t ktsan_tests_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *offset)
{
	char buffer[16];

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	if (!strcmp(buffer, "tsan_run_tests\n"))
		ktsan_run_tests();

	return count;
}

static const struct file_operations ktsan_tests_operations = {
	.write = ktsan_tests_write,
};

static int __init ktsan_tests_init(void)
{
	proc_create("ktsan_tests", S_IWUSR, NULL, &ktsan_tests_operations);
	return 0;
}

device_initcall(ktsan_tests_init);
