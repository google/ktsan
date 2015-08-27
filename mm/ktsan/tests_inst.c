#include "ktsan.h"

#include <linux/bitops.h>
#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/percpu.h>
#include <linux/preempt.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>
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

	value = kmalloc(1024, GFP_KERNEL);
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

/* ktsan test: offset. */

static int offset_first(void *arg)
{
	*((int *)arg) = 1;

	return 0;
}

static int offset_second(void *arg)
{
	*((int *)arg + 1) = 1;

	return 0;
}

static void kt_test_offset(void)
{
	kt_test(kt_nop, offset_first, offset_second,
		"offset", "no race expected");
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

static int atomic64_first(void *arg)
{
	int value = atomic64_read((atomic64_t *)arg);

	return value;
}

static int atomic64_second(void *arg)
{
	atomic64_set((atomic64_t *)arg, 1);

	return 0;
}

static int atomic_xchg_xadd_first(void *arg)
{
	return xchg((int *)arg, 42);
}

static int atomic_xchg_xadd_second(void *arg)
{
	return xadd((int *)arg, 42);
}

static void kt_test_atomic(void)
{
	kt_test(kt_nop, atomic_first, atomic_second,
		"atomic", "no race expected");
	kt_test(kt_nop, atomic64_first, atomic64_second,
		"atomic64", "no race expected");
	kt_test(kt_nop, atomic_xchg_xadd_first, atomic_xchg_xadd_second,
		"xchg & xadd", "no race expected");
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

/* ktsan tests: percpu. */

DEFINE_PER_CPU(int, percpu_var);
DEFINE_PER_CPU(int, percpu_array[128]);

static int percpu_get_put(void *arg)
{
	get_cpu_var(percpu_var) = 0;
	put_cpu_var(percpu_var);

	return 0;
}

static int percpu_irq(void *arg)
{
	unsigned long flags;

	local_irq_save(flags);
	*this_cpu_ptr(&percpu_var) = 0;
	local_irq_restore(flags);

	return 0;
}

static int percpu_preempt_array(void *arg)
{
	int i;

	preempt_disable();
	for (i = 0; i < 128; i++)
		*this_cpu_ptr(&percpu_array[i]) = i;
	preempt_enable();

	return 0;
}

/* FIXME(xairy): this test doesn't produce report at all. */
static int percpu_access_one(void *arg)
{
	preempt_disable();
	per_cpu(percpu_var, 0) = 0;
	preempt_enable();

	return 0;
}

/* FIXME(xairy): this test doesn't produce report sometimes. */
static int percpu_race(void *arg)
{
	*((int *)arg) = 1;

	preempt_disable();
	*this_cpu_ptr(&percpu_var) = 0;
	preempt_enable();

	return 0;
}

static void kt_test_percpu(void)
{
	kt_test(kt_nop, percpu_get_put, percpu_get_put,
		"percpu preempt", "no race expected");
	kt_test(kt_nop, percpu_irq, percpu_irq,
		"percpu irq", "no race expected");
	kt_test(kt_nop, percpu_preempt_array, percpu_preempt_array,
		"percpu array", "no race expected");
	kt_test(kt_nop, percpu_access_one, percpu_access_one,
		"percpu access one", "race expected");
	kt_test(kt_nop, percpu_race, percpu_race,
		"percpu race", "race expected");
}

/* ktsan test: rcu */

static int rcu_read_under_lock(void *arg)
{
	int value;

	rcu_read_lock();
	value = *((int *)arg);
	rcu_read_unlock();

	return value;
}

static int rcu_synchronize(void *arg)
{
	synchronize_rcu();
	*((int *)arg) = 0;

	return 0;
}

static int rcu_write_under_lock(void *arg)
{
	rcu_read_lock();
	*((int *)arg) = 0;
	rcu_read_unlock();

	return 0;
}

static int rcu_init_ptr(void *arg)
{
	*((int *)arg + 4) = 1;
	*(int **)arg = (int *)arg + 4;

	return 0;
}

static int rcu_assign_ptr(void *arg)
{
	*((int *)arg + 8) = 4242;
	rcu_assign_pointer(*(int **)arg, (int *)arg + 8);

	return 0;
}

static int rcu_deref_ptr(void *arg)
{
	int *ptr = rcu_dereference(*(int **)arg);
	*ptr = 42;

	return 0;
}

static void kt_test_rcu(void)
{
	kt_test(kt_nop, rcu_read_under_lock, rcu_synchronize,
		"rcu-read-synchronize", "no race expected");

	/* FIXME(xairy): this test doesn't produce report. */
	kt_test(kt_nop, rcu_write_under_lock, rcu_write_under_lock,
		"rcu-write-write", "race expected");

	/* FIXME(xairy): this test doesn't produce report. */
	kt_test(kt_nop, rcu_read_under_lock, rcu_write_under_lock,
		"rcu-read-write", "race expected");

	kt_test(kt_nop, rcu_read_under_lock, rcu_assign_ptr,
		"rcu-read-assign", "no race expected");

	kt_test(rcu_init_ptr, rcu_deref_ptr, rcu_assign_ptr,
		"rcu-deref-assign", "no race expected");
}

struct wait_on_bit_arg
{
	unsigned long bit;
	unsigned long data;
};

static int wait_on_bit_main(void *p)
{
	struct wait_on_bit_arg *arg = p;

	arg->bit = 1;
	return 0;
}

static int wait_on_bit_thr1(void *p)
{
	struct wait_on_bit_arg *arg = p;

	arg->data = 1;
	clear_bit_unlock(0, &arg->bit);
	wake_up_bit(&arg->bit, 0);
	return 0;
}

static int wait_on_bit_thr2(void *p)
{
	struct wait_on_bit_arg *arg = p;

	wait_on_bit(&arg->bit, 0, TASK_UNINTERRUPTIBLE);
	if (arg->data != 1)
		BUG_ON(1);
	return 0;
}

static void kt_test_wait_on_bit(void)
{
	kt_test(wait_on_bit_main, wait_on_bit_thr1, wait_on_bit_thr2,
		"wait_on_bit", "no race expected");
}

/* Instrumented tests. */

void kt_tests_run_inst(void)
{
	pr_err("ktsan: running instrumented tests, T%d.\n", current->pid);
	pr_err("\n");

	kt_test_race();
	pr_err("\n");
	kt_test_offset();
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
	kt_test_percpu();
	pr_err("\n");
	kt_test_rcu();
	pr_err("\n");
	kt_test_wait_on_bit();
	pr_err("\n");
}
