#include "ktsan.h"

#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>

/* Instrumented tests. */

void kt_test_race(void);
void kt_test_thread_create(void);
void kt_test_spinlock(void);
void kt_test_atomic(void);
void kt_test_completion(void);
void kt_test_mutex(void);
void kt_test_semaphore(void);

void kt_tests_run_inst(void)
{
	pr_err("TSan: running instrumented tests, thread #%d.\n", current->pid);
	pr_err("\n");

	kt_test_race();
	pr_err("\n");
	kt_test_thread_create();
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
}

/* ktsan test: race. */

DECLARE_COMPLETION(race_thr_fst_compl);
DECLARE_COMPLETION(race_thr_snd_compl);

static int race_thr_fst_func(void *arg)
{
	int value = *((char *)arg);

	complete(&race_thr_fst_compl);

	return value;
}

static int race_thr_snd_func(void *arg)
{
	*((int *)arg) = 1;

	complete(&race_thr_snd_compl);

	return 0;
}

void kt_test_race(void)
{
	struct task_struct *thr_fst, *thr_snd;
	char thr_fst_name[] = "race-thr-fst";
	char thr_snd_name[] = "race-thr-snd";
	int *value = kmalloc(32, GFP_KERNEL);

	BUG_ON(!value);

	pr_err("TSan: starting race test, race expected.\n");

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

/* ktsan test: thread create. */

DECLARE_COMPLETION(thr_crt_thr_compl);

static int thr_crt_thr_func(void *arg)
{
	*((int *)arg) = 1;

	complete(&thr_crt_thr_compl);

	return 0;
}

void kt_test_thread_create(void)
{
	struct task_struct *thr;
	char thr_name[] = "thr-crt-thr";
	int *value = kmalloc(32, GFP_KERNEL);

	BUG_ON(!value);

	pr_err("TSan: starting thread create test, no race expected.\n");

	*value = 0;

	thr = kthread_create(thr_crt_thr_func, value, thr_name);

	if (IS_ERR(thr)) {
		pr_err("TSan: could not create kernel thread.\n");
		return;
	}

	wake_up_process(thr);

	wait_for_completion(&thr_crt_thr_compl);

	kfree(value);

	pr_err("TSan: end of test.\n");
}

/* ktsan test: spinlock. */

DECLARE_COMPLETION(spinlock_thr_fst_compl);
DECLARE_COMPLETION(spinlock_thr_snd_compl);

DEFINE_SPINLOCK(spinlock_lock);

int fst_id;
int snd_id;
kt_clk_t *fst_clk;
kt_clk_t *snd_clk;

static int spinlock_thr_fst_func(void *arg)
{
	int value;

	spin_lock(&spinlock_lock);
	value = *((char *)arg);
	spin_unlock(&spinlock_lock);

	complete(&spinlock_thr_fst_compl);

	return value;
}

static int spinlock_thr_snd_func(void *arg)
{
	spin_lock(&spinlock_lock);
	*((int *)arg) = 1;
	spin_unlock(&spinlock_lock);

	complete(&spinlock_thr_snd_compl);

	return 0;
}

void kt_test_spinlock(void)
{
	struct task_struct *thr_fst, *thr_snd;
	char thr_fst_name[] = "spinlock-thr-fst";
	char thr_snd_name[] = "spinlock-thr-snd";
	/*
	 * Different kmalloc size to ensure that the address of the allocated
	 * block of memory will be different from the one in race test for now.
	 */
	int *value = kmalloc(32, GFP_KERNEL);

	BUG_ON(!value);

	pr_err("TSan: starting spinlock test, no race expected.\n");

	thr_fst = kthread_create(spinlock_thr_fst_func, value, thr_fst_name);
	thr_snd = kthread_create(spinlock_thr_snd_func, value, thr_snd_name);

	if (IS_ERR(thr_fst) || IS_ERR(thr_snd)) {
		pr_err("TSan: could not create kernel threads.\n");
		return;
	}

	fst_id = thr_fst->ktsan.thr->id;
	snd_id = thr_snd->ktsan.thr->id;
	fst_clk = &thr_fst->ktsan.thr->clk;
	snd_clk = &thr_snd->ktsan.thr->clk;

	pr_err("%d: {%d: %lu, %d: %lu}, %d: {%d: %lu, %d: %lu}\n",
		fst_id, fst_id, fst_clk->time[fst_id],
			snd_id, fst_clk->time[snd_id],
		snd_id,	fst_id, snd_clk->time[fst_id],
			snd_id, snd_clk->time[snd_id]);

	wake_up_process(thr_fst);
	wake_up_process(thr_snd);

	wait_for_completion(&spinlock_thr_fst_compl);
	wait_for_completion(&spinlock_thr_snd_compl);

	pr_err("%d: {%d: %lu, %d: %lu}, %d: {%d: %lu, %d: %lu}\n",
		fst_id, fst_id, fst_clk->time[fst_id],
			snd_id, fst_clk->time[snd_id],
		snd_id,	fst_id, snd_clk->time[fst_id],
			snd_id, snd_clk->time[snd_id]);

	kfree(value);

	pr_err("TSan: end of test.\n");
}

/* ktsan test: atomic. */

DECLARE_COMPLETION(atomic_thr_fst_compl);
DECLARE_COMPLETION(atomic_thr_snd_compl);

static int atomic_thr_fst_func(void *arg)
{
	int value = atomic_read((atomic_t *)arg);

	complete(&atomic_thr_fst_compl);

	return value;
}

static int atomic_thr_snd_func(void *arg)
{
	atomic_set((atomic_t *)arg, 1);

	complete(&atomic_thr_snd_compl);

	return 0;
}

void kt_test_atomic(void)
{
	struct task_struct *thr_fst, *thr_snd;
	char thr_fst_name[] = "atomic-thr-fst";
	char thr_snd_name[] = "atomic-thr-snd";
	int *value = kmalloc(32, GFP_KERNEL);

	BUG_ON(!value);

	pr_err("TSan: starting atomic test, no race expected.\n");

	thr_fst = kthread_create(atomic_thr_fst_func, value, thr_fst_name);
	thr_snd = kthread_create(atomic_thr_snd_func, value, thr_snd_name);

	if (IS_ERR(thr_fst) || IS_ERR(thr_snd)) {
		pr_err("TSan: could not create kernel threads.\n");
		return;
	}

	wake_up_process(thr_fst);
	wake_up_process(thr_snd);

	wait_for_completion(&atomic_thr_fst_compl);
	wait_for_completion(&atomic_thr_snd_compl);

	kfree(value);

	pr_err("TSan: end of test.\n");
}

/* ktsan test: completion. */

DECLARE_COMPLETION(compl_thr_fst_compl);
DECLARE_COMPLETION(compl_thr_snd_compl);

DECLARE_COMPLETION(compl_access_compl);

static int compl_thr_fst_func(void *arg)
{
	int value;

	wait_for_completion(&compl_access_compl);
	value = *((char *)arg);

	complete(&compl_thr_fst_compl);

	return value;
}

static int compl_thr_snd_func(void *arg)
{
	*((int *)arg) = 1;
	complete(&compl_access_compl);

	complete(&compl_thr_snd_compl);

	return 0;
}

void kt_test_completion(void)
{
	struct task_struct *thr_fst, *thr_snd;
	char thr_fst_name[] = "compl-thr-fst";
	char thr_snd_name[] = "compl-thr-snd";
	int *value = kmalloc(32, GFP_KERNEL);

	BUG_ON(!value);

	pr_err("TSan: starting completion test, no race expected.\n");

	thr_fst = kthread_create(compl_thr_fst_func, value, thr_fst_name);
	thr_snd = kthread_create(compl_thr_snd_func, value, thr_snd_name);

	if (IS_ERR(thr_fst) || IS_ERR(thr_snd)) {
		pr_err("TSan: could not create kernel threads.\n");
		return;
	}

	wake_up_process(thr_fst);
	wake_up_process(thr_snd);

	wait_for_completion(&compl_thr_fst_compl);
	wait_for_completion(&compl_thr_snd_compl);

	kfree(value);

	pr_err("TSan: end of test.\n");
}

/* ktsan test: mutex. */

DECLARE_COMPLETION(mutex_thr_fst_compl);
DECLARE_COMPLETION(mutex_thr_snd_compl);

DEFINE_MUTEX(mutex_access_mutex);

static int mutex_thr_fst_func(void *arg)
{
	int value;

	mutex_lock(&mutex_access_mutex);
	value = *((char *)arg);
	mutex_unlock(&mutex_access_mutex);

	complete(&mutex_thr_fst_compl);

	return value;
}

static int mutex_thr_snd_func(void *arg)
{
	mutex_lock(&mutex_access_mutex);
	*((int *)arg) = 1;
	mutex_unlock(&mutex_access_mutex);

	complete(&mutex_thr_snd_compl);

	return 0;
}

void kt_test_mutex(void)
{
	struct task_struct *thr_fst, *thr_snd;
	char thr_fst_name[] = "mutex-thr-fst";
	char thr_snd_name[] = "mutex-thr-snd";
	int *value = kmalloc(32, GFP_KERNEL);

	BUG_ON(!value);

	pr_err("TSan: starting mutex test, no race expected.\n");

	thr_fst = kthread_create(mutex_thr_fst_func, value, thr_fst_name);
	thr_snd = kthread_create(mutex_thr_snd_func, value, thr_snd_name);

	if (IS_ERR(thr_fst) || IS_ERR(thr_snd)) {
		pr_err("TSan: could not create kernel threads.\n");
		return;
	}

	wake_up_process(thr_fst);
	wake_up_process(thr_snd);

	wait_for_completion(&mutex_thr_fst_compl);
	wait_for_completion(&mutex_thr_snd_compl);

	kfree(value);

	pr_err("TSan: end of test.\n");
}

/* ktsan test: semaphore. */

DECLARE_COMPLETION(sema_thr_fst_compl);
DECLARE_COMPLETION(sema_thr_snd_compl);

DEFINE_SEMAPHORE(sema_access_sema);

static int sema_thr_fst_func(void *arg)
{
	int value;

	down(&sema_access_sema);
	value = *((char *)arg);
	up(&sema_access_sema);

	complete(&sema_thr_fst_compl);

	return value;
}

static int sema_thr_snd_func(void *arg)
{
	down(&sema_access_sema);
	*((int *)arg) = 1;
	up(&sema_access_sema);

	complete(&sema_thr_snd_compl);

	return 0;
}

void kt_test_semaphore(void)
{
	struct task_struct *thr_fst, *thr_snd;
	char thr_fst_name[] = "sema-thr-fst";
	char thr_snd_name[] = "sema-thr-snd";
	int *value = kmalloc(32, GFP_KERNEL);

	BUG_ON(!value);

	pr_err("TSan: starting semaphore test, no race expected.\n");

	thr_fst = kthread_create(sema_thr_fst_func, value, thr_fst_name);
	thr_snd = kthread_create(sema_thr_snd_func, value, thr_snd_name);

	if (IS_ERR(thr_fst) || IS_ERR(thr_snd)) {
		pr_err("TSan: could not create kernel threads.\n");
		return;
	}

	wake_up_process(thr_fst);
	wake_up_process(thr_snd);

	wait_for_completion(&sema_thr_fst_compl);
	wait_for_completion(&sema_thr_snd_compl);

	kfree(value);

	pr_err("TSan: end of test.\n");
}
