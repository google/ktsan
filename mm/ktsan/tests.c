#include "ktsan.h"

#include <linux/completion.h>
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

DECLARE_COMPLETION(race_thr_fst_compl);
DECLARE_COMPLETION(race_thr_snd_compl);

static int race_thr_fst_func(void *arg)
{
	int *value = (int *)arg;

	do {
		ktsan_read2((char *)arg + 1);
		schedule();
	} while (*value == 0);
	ktsan_write2((char *)arg + 1);
	*value = 0;

	complete(&race_thr_fst_compl);

	return *value;
}

static int race_thr_snd_func(void *arg)
{
	int *value = (int *)arg;

	ktsan_write4(arg);
	*value = 1;

	complete(&race_thr_snd_compl);

	return *value;
}

static void ktsan_test_race(void)
{
	struct task_struct *thr_fst, *thr_snd;
	char thr_fst_name[] = "race-thr-fst";
	char thr_snd_name[] = "race-thr-snd";
	int *value = kmalloc(sizeof(int), GFP_KERNEL);

	BUG_ON(!value);

	pr_err("TSan: starting test, race expected.\n");

	thr_fst = kthread_create(race_thr_fst_func, value, thr_fst_name);
	thr_snd = kthread_create(race_thr_snd_func, value, thr_snd_name);

	if (IS_ERR(thr_fst) || IS_ERR(thr_snd)) {
		pr_err("TSan: could not create kernel threads.\n");
		return;
	}

	wake_up_process(thr_fst);
	wake_up_process(thr_snd);

	wait_for_completion(&race_thr_fst_compl);
	wait_for_completion(&race_thr_snd_compl);

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
