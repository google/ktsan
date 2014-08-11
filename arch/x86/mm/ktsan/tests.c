#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/string.h>

#include <asm/uaccess.h>
#include <asm/thread_info.h>

/* KTsan test: race. */

static int thread_race(void *arg)
{
	int i;
	int *value = (int *)arg;

	for (i = 0; i < 500 * 1000 * 1000; i++)
		*value++;

	return value;
}

static void ktsan_test_race(void)
{
	struct task_struct *thread_first, *thread_second;

	char thread_name_first[] = "thread-race-first";
	char thread_name_second[] = "thread-race-second";

	int value; /* Allocated on stack => in physical memory. */

	pr_err("TSan: starting test, race expected.\n");

	thread_first = kthread_create(thread_race, &value, thread_name_first);
	thread_second = kthread_create(thread_race, &value, thread_name_second);

	if (!thread_first || !thread_second) {
		pr_err("TSan: could not create kernel threads.\n");
		return;
	}

	wake_up_process(thread_first);
	wake_up_process(thread_second);

	kthread_stop(thread_first);
	kthread_stop(thread_second);

	pr_err("TSan: end of test.\n");
}

/* KTSan test: no race. */

static void ktsan_test_no_race(void)
{

}

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
