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

typedef void (*thr_func_t)(void *);

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

static volatile int always_false;
static noinline void use(int x)
{
	if (always_false)
		always_false = x;
}

DECLARE_COMPLETION(thr_fst_compl);
DECLARE_COMPLETION(thr_snd_compl);

void kt_test(thr_func_t main, thr_func_t first, thr_func_t second,
		const char *name, bool has_race)
{
	struct task_struct *thr_fst, *thr_snd;
	char thr_fst_name[] = "thr-fst";
	char thr_snd_name[] = "thr-snd";
	thr_arg_t thr_fst_arg, thr_snd_arg;
	int *value, i;

	pr_err("ktsan: starting %s test, %s.\n", name,
		has_race ? "race expected" : "no race expected");

	// Run each test 10 times.
	// Due to racy race detection algorithm tsan can miss races sometimes,
	// so we require it to catch a race at least once in 10 runs.
	// For tests without races, it would not be out of place to ensure
	// that no runs result in false race reports.
	for (i = 0; i < 10; i++) {
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
	}

	pr_err("ktsan: end of test.\n");
}

static void kt_nop(void *arg) {}

/* ktsan test: race. */

static void race_first(void *arg)
{
	use(*((char *)arg));
}

static void race_second(void *arg)
{
	*((int *)arg) = 1;
}

static void kt_test_race(void)
{
	kt_test(kt_nop, race_first, race_second, "race", true);
}

/* ktsan test: offset. */

static void offset_first(void *arg)
{
	*((int *)arg) = 1;
}

static void offset_second(void *arg)
{
	*((int *)arg + 1) = 1;
}

static void kt_test_offset(void)
{
	kt_test(kt_nop, offset_first, offset_second,
		"offset", false);
}

/* ktsan test: spinlock. */

DEFINE_SPINLOCK(spinlock_sync);

static void spinlock_first(void *arg)
{
	spin_lock(&spinlock_sync);
	use(*((char *)arg));
	spin_unlock(&spinlock_sync);
}

static void spinlock_second(void *arg)
{
	spin_lock(&spinlock_sync);
	*((int *)arg) = 1;
	spin_unlock(&spinlock_sync);
}

static void kt_test_spinlock(void)
{
	kt_test(kt_nop, spinlock_first, spinlock_second,
		"spinlock", false);
}

/* ktsan test: READ_ONCE_CTRL. */

struct roc_arg {
	unsigned long sync;
	unsigned long data;
};

static void roc_init(void *p)
{
	struct roc_arg *arg = p;
	
	arg->sync = arg->data = 0;
}

static void roc_write_wmb(void *p)
{
	struct roc_arg *arg = p;

	arg->data = 1;
	smp_wmb();
	WRITE_ONCE(arg->sync, 1);
}

static void roc_read(void *p)
{
	struct roc_arg *arg = p;

	if (READ_ONCE(arg->sync)) {
		arg->data = 2;
	}
}

static void roc_read_ctrl(void *p)
{
	struct roc_arg *arg = p;

	if (READ_ONCE_CTRL(arg->sync)) {
		arg->data = 2;
	}
}

static void kt_test_read_once_ctrl(void)
{
	kt_test(roc_init, roc_write_wmb, roc_read,
		"READ_ONCE[_CTRL]", true);
	kt_test(roc_init, roc_write_wmb, roc_read_ctrl,
		"READ_ONCE_CTRL", false);
}

/* ktsan test: atomic. */

static void atomic_first(void *arg)
{
	use(atomic_read((atomic_t *)arg));
}

static void atomic_second(void *arg)
{
	atomic_set((atomic_t *)arg, 1);
}

static void atomic64_first(void *arg)
{
	use(atomic64_read((atomic64_t *)arg));
}

static void atomic64_second(void *arg)
{
	atomic64_set((atomic64_t *)arg, 1);
}

static void atomic_xchg_xadd_first(void *arg)
{
	xchg((int *)arg, 42);
}

static void atomic_xchg_xadd_second(void *arg)
{
	xadd((int *)arg, 42);
}

static void kt_test_atomic(void)
{
	kt_test(kt_nop, atomic_first, atomic_second,
		"atomic", false);
	kt_test(kt_nop, atomic64_first, atomic64_second,
		"atomic64", false);
	kt_test(kt_nop, atomic_xchg_xadd_first, atomic_xchg_xadd_second,
		"xchg & xadd", false);
}

/* ktsan test: completion. */

DECLARE_COMPLETION(completion_sync);

static void compl_first(void *arg)
{
	wait_for_completion(&completion_sync);
	use(*((char *)arg));
}

static void compl_second(void *arg)
{
	*((int *)arg) = 1;
	complete(&completion_sync);
}

static void kt_test_completion(void)
{
	kt_test(kt_nop, compl_first, compl_second,
		"completion", false);
}

/* ktsan test: mutex. */

DEFINE_MUTEX(mutex_sync);

static void mutex_first(void *arg)
{
	mutex_lock(&mutex_sync);
	use(*((char *)arg));
	mutex_unlock(&mutex_sync);
}

static void mutex_second(void *arg)
{
	mutex_lock(&mutex_sync);
	*((int *)arg) = 1;
	mutex_unlock(&mutex_sync);
}

static void kt_test_mutex(void)
{
	kt_test(kt_nop, mutex_first, mutex_second,
		"mutex", false);
}

/* ktsan test: semaphore. */

DEFINE_SEMAPHORE(sema_sync);

static void sema_first(void *arg)
{
	down(&sema_sync);
	*((int *)arg) = 2;
	up(&sema_sync);
}

static void sema_second(void *arg)
{
	down(&sema_sync);
	*((int *)arg) = 1;
	up(&sema_sync);
}

static void kt_test_semaphore(void)
{
	kt_test(kt_nop, sema_first, sema_second,
		"semaphore", false);
}

/* ktsan test: rwlock. */

DEFINE_RWLOCK(rwlock_sync);

static void rwlock_first(void *arg)
{
	write_lock(&rwlock_sync);
	*((int *)arg) = 1;
	write_unlock(&rwlock_sync);
}

static void rwlock_second(void *arg)
{
	write_lock(&rwlock_sync);
	*((int *)arg) = 1;
	write_unlock(&rwlock_sync);
}

static void kt_test_rwlock(void)
{
	kt_test(kt_nop, rwlock_first, rwlock_second,
		"rwlock", false);
}

/* ktsan test: rwsem. */

DECLARE_RWSEM(rwsem_sync);

static void rwsem_first(void *arg)
{
	down_write(&rwsem_sync);
	*((int *)arg) = 1;
	up_write(&rwsem_sync);
}

static void rwsem_second(void *arg)
{
	down_write(&rwsem_sync);
	*((int *)arg) = 1;
	up_write(&rwsem_sync);
}

static void kt_test_rwsem(void)
{
	kt_test(kt_nop, rwsem_first, rwsem_second,
		"rwsem", false);
}

/* ktsan test: thread create. */

static void thr_crt_main(void *arg)
{
	*((int *)arg) = 1;
}

static void thr_crt_first(void *arg)
{
	*((int *)arg) = 1;
}

static void kt_test_thread_create(void)
{
	kt_test(thr_crt_main, thr_crt_first, kt_nop,
		"thread creation", false);
}

/* ktsan tests: percpu. */

DEFINE_PER_CPU(int, percpu_var);
DEFINE_PER_CPU(int, percpu_array[128]);

static void percpu_get_put(void *arg)
{
	get_cpu_var(percpu_var) = 0;
	put_cpu_var(percpu_var);
}

static void percpu_irq(void *arg)
{
	unsigned long flags;

	local_irq_save(flags);
	*this_cpu_ptr(&percpu_var) = 0;
	local_irq_restore(flags);
}

static void percpu_preempt_array(void *arg)
{
	int i;

	preempt_disable();
	for (i = 0; i < 128; i++)
		*this_cpu_ptr(&percpu_array[i]) = i;
	preempt_enable();
}

/* FIXME(xairy): this test doesn't produce report at all. */
static void percpu_access_one(void *arg)
{
	preempt_disable();
	per_cpu(percpu_var, 0) = 0;
	preempt_enable();
}

/* FIXME(xairy): this test doesn't produce report sometimes. */
static void percpu_race(void *arg)
{
	*((int *)arg) = 1;

	preempt_disable();
	*this_cpu_ptr(&percpu_var) = 0;
	preempt_enable();
}

static void kt_test_percpu(void)
{
	kt_test(kt_nop, percpu_get_put, percpu_get_put,
		"percpu preempt", false);
	kt_test(kt_nop, percpu_irq, percpu_irq,
		"percpu irq", false);
	kt_test(kt_nop, percpu_preempt_array, percpu_preempt_array,
		"percpu array", false);
	kt_test(kt_nop, percpu_access_one, percpu_access_one,
		"percpu access one", true);
	kt_test(kt_nop, percpu_race, percpu_race,
		"percpu race", true);
}

/* ktsan test: rcu */

static void rcu_read_under_lock(void *arg)
{
	rcu_read_lock();
	use(*((int *)arg));
	rcu_read_unlock();
}

static void rcu_synchronize(void *arg)
{
	synchronize_rcu();
	*((int *)arg) = 0;
}

static void rcu_write_under_lock(void *arg)
{
	rcu_read_lock();
	*((int *)arg) = 0;
	rcu_read_unlock();
}

static void rcu_init_ptr(void *arg)
{
	*((int *)arg + 4) = 1;
	*(int **)arg = (int *)arg + 4;
}

static void rcu_assign_ptr(void *arg)
{
	*((int *)arg + 8) = 4242;
	rcu_assign_pointer(*(int **)arg, (int *)arg + 8);
}

static void rcu_deref_ptr(void *arg)
{
	int *ptr = rcu_dereference(*(int **)arg);
	*ptr = 42;
}

static void kt_test_rcu(void)
{
	kt_test(kt_nop, rcu_read_under_lock, rcu_synchronize,
		"rcu-read-synchronize", false);

	/* FIXME(xairy): this test doesn't produce report. */
	kt_test(kt_nop, rcu_write_under_lock, rcu_write_under_lock,
		"rcu-write-write", true);

	/* FIXME(xairy): this test doesn't produce report. */
	kt_test(kt_nop, rcu_read_under_lock, rcu_write_under_lock,
		"rcu-read-write", true);

	kt_test(kt_nop, rcu_read_under_lock, rcu_assign_ptr,
		"rcu-read-assign", false);

	kt_test(rcu_init_ptr, rcu_deref_ptr, rcu_assign_ptr,
		"rcu-deref-assign", false);
}

/* ktsan test: seqlock */

struct wait_on_bit_arg
{
	unsigned long bit;
	unsigned long data;
};

static void wait_on_bit_main(void *p)
{
	struct wait_on_bit_arg *arg = p;

	arg->bit = 1;
}

static void wait_on_bit_thr1(void *p)
{
	struct wait_on_bit_arg *arg = p;

	arg->data = 1;
	clear_bit_unlock(0, &arg->bit);
	wake_up_bit(&arg->bit, 0);
}

static void wait_on_bit_thr2(void *p)
{
	struct wait_on_bit_arg *arg = p;

	wait_on_bit(&arg->bit, 0, TASK_UNINTERRUPTIBLE);
	if (arg->data != 1)
		BUG_ON(1);
}

static void kt_test_wait_on_bit(void)
{
	kt_test(wait_on_bit_main, wait_on_bit_thr1, wait_on_bit_thr2,
		"wait_on_bit", false);
}

struct seqcount_arg
{
	seqcount_t seq[3];
	int data[6];
};

static void seq_main(void *p)
{
	struct seqcount_arg *arg = p;
	int i;

	for (i = 0; i < ARRAY_SIZE(arg->seq); i++)
		seqcount_init(&arg->seq[i]);
	for (i = 0; i < ARRAY_SIZE(arg->data); i++)
		arg->data[i] = 0;
}

static void seq_write(void *p)
{
	struct seqcount_arg *arg = p;
	int i;

	for (i = 0; i < 1000; i++) {
		write_seqcount_begin(&arg->seq[0]);
		arg->data[0]++;
		arg->data[1]++;
		arg->data[2]++;
		arg->data[3]++;
		write_seqcount_end(&arg->seq[0]);
	}
}

static void seq_read1(void *p)
{
	struct seqcount_arg *arg = p;
	unsigned seq;
	int sum, i;

	for (i = 0; i < 1000; i++) {
		do {
			sum = 0;
			seq = __read_seqcount_begin(&arg->seq[0]);
			rmb();
			sum = arg->data[0] + arg->data[1] +
				arg->data[2] + arg->data[3];
			rmb();
		} while (__read_seqcount_retry(&arg->seq[0], seq));
		BUG_ON((sum % 4) != 0);
	}
}

static void seq_read2(void *p)
{
	struct seqcount_arg *arg = p;
	unsigned seq;
	int sum, i;

	for (i = 0; i < 1000; i++) {
		do {
			sum = 0;
			seq = read_seqcount_begin(&arg->seq[0]);
			sum = arg->data[0] + arg->data[1] +
				arg->data[2] + arg->data[3];
		} while (read_seqcount_retry(&arg->seq[0], seq));
		BUG_ON((sum % 4) != 0);
	}
}

static void seq_read3(void *p)
{
	struct seqcount_arg *arg = p;
	unsigned seq;
	int sum, i;

	for (i = 0; i < 1000; i++) {
		do {
			sum = 0;
			seq = raw_read_seqcount_latch(&arg->seq[0]);
			sum = arg->data[0] + arg->data[1] +
				arg->data[2] + arg->data[3];
		} while (read_seqcount_retry(&arg->seq[0], seq));
		/* don't BUG_ON, we use latch incorrectly */
		use((sum % 4) != 0);
	}
}

static void seq_write4(void *p)
{
	struct seqcount_arg *arg = p;
	int i;

	for (i = 0; i < 1000; i++) {
		write_seqcount_begin(&arg->seq[0]);
		arg->data[0]++;
		arg->data[1]++;
		write_seqcount_end(&arg->seq[0]);

		write_seqcount_begin(&arg->seq[1]);
		arg->data[2]++;
		arg->data[3]++;
		write_seqcount_end(&arg->seq[1]);

		write_seqcount_begin(&arg->seq[2]);
		arg->data[4]++;
		arg->data[5]++;
		write_seqcount_end(&arg->seq[2]);
	}
}

static void seq_read4(void *p)
{
	struct seqcount_arg *arg = p;
	unsigned seq[3];
	int sum, i;

	for (i = 0; i < 1000; i++) {
		/*
		 * A crazy mix of nested and overlapping read critical sections.
		 * fs/namei.c:path_init actually does this.
		 */
		for (;;) {
			seq[0] = read_seqcount_begin(&arg->seq[0]);
			for (;;) {
				sum = 0;
				seq[1] = read_seqcount_begin(&arg->seq[1]);
				sum = arg->data[2] + arg->data[3];
				seq[2] = read_seqcount_begin(&arg->seq[2]);
				if (read_seqcount_retry(&arg->seq[1], seq[1])) {
					read_seqcount_cancel(&arg->seq[2]);
					continue;
				}
				break;
			}
			sum += arg->data[4] + arg->data[5];
			if (read_seqcount_retry(&arg->seq[2], seq[2])) {
				read_seqcount_cancel(&arg->seq[0]);
				continue;
			}
			sum += arg->data[0] + arg->data[1];
			if (read_seqcount_retry(&arg->seq[0], seq[0]))
				continue;
			break;
		}
		use(sum);
	}
}

static void seq_read_cancel(void *p)
{
	struct seqcount_arg *arg = p;
	unsigned seq;
	int sum, i;

	for (i = 0; i < 1000; i++) {
		do {
			sum = 0;
			seq = read_seqcount_begin(&arg->seq[0]);
			sum = arg->data[0] + arg->data[1] +
				arg->data[2] + arg->data[3];
			if (sum != 0) {
				read_seqcount_cancel(&arg->seq[0]);
				break;
			}
		} while (read_seqcount_retry(&arg->seq[0], seq));
		use(sum);
	}
}

static void kt_test_seqcount(void)
{
	kt_test(seq_main, seq_write, seq_read1, "seqcount1", false);
	kt_test(seq_main, seq_write, seq_read2, "seqcount2", false);
	kt_test(seq_main, seq_write, seq_read3, "seqcount3", false);
	kt_test(seq_main, seq_write4, seq_read4, "seqcount4", false);
	kt_test(seq_main, seq_write, seq_read_cancel, "seqcount_cancel", false);
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
	kt_test_read_once_ctrl();
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
	kt_test_seqcount();
	pr_err("\n");
}
