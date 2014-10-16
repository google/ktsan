#include "ktsan.h"

#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/rwlock.h>
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>

typedef int (*thr_func_t)(void *);

struct thr_arg_s {
	void *value;
	thr_func_t func;
	struct completion *completion;
};

typedef struct thr_arg_s thr_arg_t;

int thr_func(void *arg)
{
	thr_arg_t *thr_arg = (thr_arg_t *)arg;

	thr_arg->func(thr_arg->value);
	complete(thr_arg->completion);

	return 0;
}

DECLARE_COMPLETION(thr_fst_compl);
DECLARE_COMPLETION(thr_snd_compl);

void kt_test(thr_func_t main, thr_func_t first, thr_func_t second,
		const char *name, const char *result)
{
	struct task_struct *thr_fst, *thr_snd;
	char thr_fst_name[] = "thr-fst";
	char thr_snd_name[] = "thr-snd";
	thr_arg_t thr_fst_arg, thr_snd_arg;
	int *value;

	pr_err("ktsan: starting %s test, %s.\n", name, result);

	value = kmalloc(32, GFP_KERNEL);
	BUG_ON(!value);

	main(value);

	reinit_completion(&thr_fst_compl);
	reinit_completion(&thr_snd_compl);

	thr_fst_arg.value = value;
	thr_fst_arg.func = first;
	thr_fst_arg.completion = &thr_fst_compl;

	thr_snd_arg.value = value;
	thr_snd_arg.func = second;
	thr_snd_arg.completion = &thr_snd_compl;

	thr_fst = kthread_create(thr_func, &thr_fst_arg, thr_fst_name);
	thr_snd = kthread_create(thr_func, &thr_snd_arg, thr_snd_name);

	if (IS_ERR(thr_fst) || IS_ERR(thr_snd)) {
		pr_err("ktsan: could not create kernel threads.\n");
		return;
	}

	wake_up_process(thr_fst);
	wake_up_process(thr_snd);

	wait_for_completion(&thr_fst_compl);
	wait_for_completion(&thr_snd_compl);

	kfree(value);

	pr_err("ktsan: end of test.\n");
}

static int kt_nop(void *arg) { return 0; }

/* ktsan test: race. */

static int race_first(void *arg)
{
	int value = *((char *)arg);

	return value;
}

static int race_second(void *arg)
{
	*((int *)arg) = 1;

	return 0;
}

static void kt_test_race(void)
{
	kt_test(kt_nop, race_first, race_second, "race", "race expected");
}

/* ktsan test: spinlock. */

DEFINE_SPINLOCK(spinlock_sync);

static int spinlock_first(void *arg)
{
	int value;

	spin_lock(&spinlock_sync);
	value = *((char *)arg);
	spin_unlock(&spinlock_sync);

	return value;
}

static int spinlock_second(void *arg)
{
	spin_lock(&spinlock_sync);
	*((int *)arg) = 1;
	spin_unlock(&spinlock_sync);

	return 0;
}

static void kt_test_spinlock(void)
{
	kt_test(kt_nop, spinlock_first, spinlock_second,
		"spinlock", "no race expected");
}

/* ktsan test: atomic. */

static int atomic_first(void *arg)
{
	int value = atomic_read((atomic_t *)arg);

	return value;
}

static int atomic_second(void *arg)
{
	atomic_set((atomic_t *)arg, 1);

	return 0;
}

static void kt_test_atomic(void)
{
	kt_test(kt_nop, atomic_first, atomic_second,
		"atomic", "no race expected");
}

/* ktsan test: completion. */

DECLARE_COMPLETION(completion_sync);

static int compl_first(void *arg)
{
	int value;

	wait_for_completion(&completion_sync);
	value = *((char *)arg);

	return value;
}

static int compl_second(void *arg)
{
	*((int *)arg) = 1;
	complete(&completion_sync);

	return 0;
}

static void kt_test_completion(void)
{
	kt_test(kt_nop, compl_first, compl_second,
		"completion", "no race expected");
}

/* ktsan test: mutex. */

DEFINE_MUTEX(mutex_sync);

static int mutex_first(void *arg)
{
	int value;

	mutex_lock(&mutex_sync);
	value = *((char *)arg);
	mutex_unlock(&mutex_sync);

	return value;
}

static int mutex_second(void *arg)
{
	mutex_lock(&mutex_sync);
	*((int *)arg) = 1;
	mutex_unlock(&mutex_sync);

	return 0;
}

static void kt_test_mutex(void)
{
	kt_test(kt_nop, mutex_first, mutex_second,
		"mutex", "no race expected");
}

/* ktsan test: semaphore. */

DEFINE_SEMAPHORE(sema_sync);

static int sema_first(void *arg)
{
	int value;

	down(&sema_sync);
	value = *((char *)arg);
	up(&sema_sync);

	return value;
}

static int sema_second(void *arg)
{
	down(&sema_sync);
	*((int *)arg) = 1;
	up(&sema_sync);

	return 0;
}

static void kt_test_semaphore(void)
{
	kt_test(kt_nop, sema_first, sema_second,
		"semaphore", "no race expected");
}

/* ktsan test: rwlock. */

DEFINE_RWLOCK(rwlock_sync);

static int rwlock_first(void *arg)
{
	write_lock(&rwlock_sync);
	*((int *)arg) = 1;
	write_unlock(&rwlock_sync);

	return 0;
}

static int rwlock_second(void *arg)
{
	write_lock(&rwlock_sync);
	*((int *)arg) = 1;
	write_unlock(&rwlock_sync);

	return 0;
}

static void kt_test_rwlock(void)
{
	kt_test(kt_nop, rwlock_first, rwlock_second,
		"rwlock", "no race expected");
}

/* ktsan test: rwsem. */

DECLARE_RWSEM(rwsem_sync);

static int rwsem_first(void *arg)
{
	down_write(&rwsem_sync);
	*((int *)arg) = 1;
	up_write(&rwsem_sync);

	return 0;
}

static int rwsem_second(void *arg)
{
	down_write(&rwsem_sync);
	*((int *)arg) = 1;
	up_write(&rwsem_sync);

	return 0;
}

static void kt_test_rwsem(void)
{
	kt_test(kt_nop, rwsem_first, rwsem_second,
		"rwsem", "no race expected");
}

/* ktsan test: thread create. */

static int thr_crt_main(void *arg)
{
	*((int *)arg) = 1;

	return 0;
}

static int thr_crt_first(void *arg)
{
	*((int *)arg) = 1;

	return 0;
}

static void kt_test_thread_create(void)
{
	kt_test(thr_crt_main, thr_crt_first, kt_nop,
		"thread creation", "no race expected");
}

/* Instrumented tests. */

void kt_tests_run_inst(void)
{
	pr_err("ktsan: running instrumented tests, T%d.\n", current->pid);
	pr_err("\n");

	kt_test_race();
	pr_err("\n");
	kt_test_spinlock();
	pr_err("\n");
	kt_test_atomic();
	pr_err("\n");
	kt_test_completion();
	pr_err("\n");
	kt_test_mutex();
	pr_err("\n");
	kt_test_semaphore();
	pr_err("\n");
	kt_test_rwlock();
	pr_err("\n");
	kt_test_rwsem();
	pr_err("\n");
	kt_test_thread_create();
	pr_err("\n");
}
