#include "ktsan.h"

#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>

DECLARE_COMPLETION(thr_fst_compl);
DECLARE_COMPLETION(thr_snd_compl);

#define DECLARE_TEST(name, main, first, second, result)			\
int kt_test_fst_ ## name (void *arg)					\
{									\
	first(arg);							\
	complete(&thr_fst_compl);					\
	return 0;							\
}									\
									\
int kt_test_snd_ ## name (void *arg)					\
{									\
	second(arg);							\
	complete(&thr_snd_compl);					\
	return 0;							\
}									\
									\
void kt_test_ ## name (void)						\
{									\
	struct task_struct *thr_fst, *thr_snd;				\
	char thr_fst_name[] = #name "-thr-fst";				\
	char thr_snd_name[] = #name "-thr-snd";				\
	int *value;							\
									\
	pr_err("ktsan: starting " #name " test, " result ".\n");	\
									\
	value = kmalloc(32, GFP_KERNEL);				\
	BUG_ON(!value);							\
									\
	main(value);							\
									\
	reinit_completion(&thr_fst_compl);				\
	reinit_completion(&thr_snd_compl);				\
									\
	thr_fst = kthread_create(kt_test_fst_ ## name,			\
				 value, thr_fst_name);			\
	thr_snd = kthread_create(kt_test_snd_ ## name,			\
				 value, thr_snd_name);			\
									\
	if (IS_ERR(thr_fst) || IS_ERR(thr_snd)) {			\
		pr_err("ktsan: could not create kernel threads.\n");	\
		return;							\
	}								\
									\
	wake_up_process(thr_fst);					\
	wake_up_process(thr_snd);					\
									\
	wait_for_completion(&thr_fst_compl);				\
	wait_for_completion(&thr_snd_compl);				\
									\
	kfree(value);							\
									\
	pr_err("ktsan: end of test.\n");				\
}									\
/**/

static void kt_nop(void* arg) { }

/* ktsan test: race. */

static int race_first(void *arg)
{
	int value = *((char *)arg);
	return value;
}

static void race_second(void *arg)
{
	*((int *)arg) = 1;
}

DECLARE_TEST(race, kt_nop, race_first, race_second, "race expected");

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

static void spinlock_second(void *arg)
{
	spin_lock(&spinlock_sync);
	*((int *)arg) = 1;
	spin_unlock(&spinlock_sync);
}

DECLARE_TEST(spinlock, kt_nop, spinlock_first, spinlock_second, "no race expected");

/* ktsan test: atomic. */

static int atomic_first(void *arg)
{
	int value = atomic_read((atomic_t *)arg);
	return value;
}

static void atomic_second(void *arg)
{
	atomic_set((atomic_t *)arg, 1);
}

DECLARE_TEST(atomic, kt_nop, atomic_first, atomic_second, "no race expected");

/* ktsan test: completion. */

DECLARE_COMPLETION(completion_sync);

static int compl_first(void *arg)
{
	int value;

	wait_for_completion(&completion_sync);
	value = *((char *)arg);

	return value;
}

static void compl_second(void *arg)
{
	*((int *)arg) = 1;
	complete(&completion_sync);
}

DECLARE_TEST(completion, kt_nop, compl_first, compl_second, "no race expected");

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

static void mutex_second(void *arg)
{
	mutex_lock(&mutex_sync);
	*((int *)arg) = 1;
	mutex_unlock(&mutex_sync);
}

DECLARE_TEST(mutex, kt_nop, mutex_first, mutex_second, "no race expected");

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

static void sema_second(void *arg)
{
	down(&sema_sync);
	*((int *)arg) = 1;
	up(&sema_sync);
}

DECLARE_TEST(semaphore, kt_nop, sema_first, sema_second, "no race expected");

/* ktsan test: thread create. */

static void thr_crt_main(void *arg)
{
	*((int *)arg) = 1;
}

static void thr_crt_first(void *arg)
{
	*((int *)arg) = 1;
}

DECLARE_TEST(thread_create, thr_crt_main, thr_crt_first, kt_nop, "no race expected");

/* Instrumented tests. */

void kt_tests_run_inst(void)
{
	pr_err("ktsan: running instrumented tests, thread #%d.\n", current->pid);
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
	kt_test_thread_create();
	pr_err("\n");
}
