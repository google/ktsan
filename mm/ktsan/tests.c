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
#include <linux/uaccess.h>

#define KT_TEST_READ_1(addr) \
	(ktsan_read1(addr), *((char *)addr))

#define KT_TEST_WRITE_4(addr, value) \
	(ktsan_write4(addr), *((int *)addr) = (value))

#define KT_TEST_SPIN_LOCK(lock, thr_clk, lock_clk)		\
	do {							\
		spin_lock(lock);				\
		kt_clk_acquire(NULL, thr_clk, lock_clk);	\
	} while (0)

#define KT_TEST_SPIN_UNLOCK(lock, thr_clk, lock_clk)		\
	do {							\
		kt_clk_release(NULL, thr_clk, lock_clk);	\
		spin_unlock(lock);				\
	} while (0)

/* KTsan test: race. */

DECLARE_COMPLETION(race_thr_fst_compl);
DECLARE_COMPLETION(race_thr_snd_compl);

static int race_thr_fst_func(void *arg)
{
	int value = KT_TEST_READ_1(arg);

	complete(&race_thr_fst_compl);

	return value;
}

static int race_thr_snd_func(void *arg)
{
	KT_TEST_WRITE_4(arg, 1);

	complete(&race_thr_snd_compl);

	return 0;
}

static void kt_test_race(void)
{
	struct task_struct *thr_fst, *thr_snd;
	char thr_fst_name[] = "race-thr-fst";
	char thr_snd_name[] = "race-thr-snd";
	int *value = kmalloc(sizeof(int), GFP_KERNEL);

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

/* KTSan test: no race. */

DECLARE_COMPLETION(spinlock_thr_fst_compl);
DECLARE_COMPLETION(spinlock_thr_snd_compl);

DEFINE_SPINLOCK(spinlock_lock);
kt_clk_t spinlock_lock_clk;

int fst_id;
int snd_id;
kt_clk_t *fst_clk;
kt_clk_t *snd_clk;

static int spinlock_thr_fst_func(void *arg)
{
	int value;
	kt_thr_t *thr = current->ktsan.thr;

	KT_TEST_SPIN_LOCK(&spinlock_lock, &thr->clk, &spinlock_lock_clk);
	value = KT_TEST_READ_1(arg);
	KT_TEST_SPIN_UNLOCK(&spinlock_lock, &thr->clk, &spinlock_lock_clk);

	complete(&spinlock_thr_fst_compl);

	return value;
}

static int spinlock_thr_snd_func(void *arg)
{
	kt_thr_t *thr = current->ktsan.thr;

	KT_TEST_SPIN_LOCK(&spinlock_lock, &thr->clk, &spinlock_lock_clk);
	KT_TEST_WRITE_4(arg, 1);
	KT_TEST_SPIN_UNLOCK(&spinlock_lock, &thr->clk, &spinlock_lock_clk);

	complete(&spinlock_thr_snd_compl);

	return 0;
}

static void kt_test_spinlock(void)
{
	struct task_struct *thr_fst, *thr_snd;
	char thr_fst_name[] = "spinlock-thr-fst";
	char thr_snd_name[] = "spinlock-thr-snd";
	/*
	 * Different kmalloc size to ensure that the address of the allocated
	 * block of memory will be different from the one in race test for now.
	 */
	int *value = kmalloc(128, GFP_KERNEL);

	BUG_ON(!value);

	pr_err("TSan: starting spinlock test, no race expected.\n");

	kt_clk_init(NULL, &spinlock_lock_clk);

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

 /* Hash table test. */

struct kt_tab_test_s {
	kt_tab_obj_t tab;
	unsigned long data[4];
};

typedef struct kt_tab_test_s kt_tab_test_t;

static void kt_test_hash_table(void)
{
	kt_tab_t tab;
	kt_tab_test_t *obj, *obj1, *obj2, *obj3;
	bool created;

	pr_err("TSan: starting hash table test.\n");

	kt_tab_init(&tab, 13, sizeof(kt_tab_test_t));

	obj = kt_tab_access(&tab, 10, NULL, false);
	BUG_ON(obj != NULL);

	/* Creating. */

	obj = kt_tab_access(&tab, 7, &created, false);
	BUG_ON(obj == NULL);
	BUG_ON(created != true);
	BUG_ON(!spin_is_locked(&obj->tab.lock));
	spin_unlock(&obj->tab.lock);

	obj1 = kt_tab_access(&tab, 7, &created, false);
	BUG_ON(obj1 != obj);
	BUG_ON(created != false);
	BUG_ON(!spin_is_locked(&obj1->tab.lock));
	spin_unlock(&obj1->tab.lock);

	obj2 = kt_tab_access(&tab, 7 + 13, &created, false);
	BUG_ON(obj2 == NULL);
	BUG_ON(obj2 == obj1);
	BUG_ON(created != true);
	BUG_ON(!spin_is_locked(&obj2->tab.lock));
	spin_unlock(&obj2->tab.lock);

	obj3 = kt_tab_access(&tab, 7 + 13, NULL, false);
	BUG_ON(obj3 != obj2);
	BUG_ON(!spin_is_locked(&obj3->tab.lock));
	spin_unlock(&obj3->tab.lock);

	obj3 = kt_tab_access(&tab, 3, &created, false);
	BUG_ON(obj3 == NULL);
	BUG_ON(obj3 == obj1 || obj3 == obj2);
	BUG_ON(created != true);
	BUG_ON(!spin_is_locked(&obj3->tab.lock));
	spin_unlock(&obj3->tab.lock);

	/* Accessing. */

	obj = kt_tab_access(&tab, 7, NULL, false);
	BUG_ON(obj == NULL);
	BUG_ON(obj != obj1);
	BUG_ON(!spin_is_locked(&obj->tab.lock));
	spin_unlock(&obj->tab.lock);

	obj = kt_tab_access(&tab, 7 + 13, &created, false);
	BUG_ON(obj == NULL);
	BUG_ON(obj != obj2);
	BUG_ON(created != false);
	BUG_ON(!spin_is_locked(&obj->tab.lock));
	spin_unlock(&obj->tab.lock);

	obj = kt_tab_access(&tab, 3, NULL, false);
	BUG_ON(obj == NULL);
	BUG_ON(obj != obj3);
	BUG_ON(!spin_is_locked(&obj->tab.lock));
	spin_unlock(&obj->tab.lock);

	/* Destriying. */

	obj = kt_tab_access(&tab, 3, NULL, true);
	BUG_ON(obj == NULL);
	BUG_ON(obj != obj3);
	BUG_ON(!spin_is_locked(&obj3->tab.lock));
	spin_unlock(&obj3->tab.lock);
	kt_cache_free(&tab.cache, obj3);

	obj = kt_tab_access(&tab, 7 + 13, NULL, true);
	BUG_ON(obj == NULL);
	BUG_ON(obj != obj2);
	BUG_ON(!spin_is_locked(&obj2->tab.lock));
	spin_unlock(&obj2->tab.lock);
	kt_cache_free(&tab.cache, obj2);

	obj = kt_tab_access(&tab, 7, NULL, true);
	BUG_ON(obj == NULL);
	BUG_ON(obj != obj1);
	BUG_ON(!spin_is_locked(&obj1->tab.lock));
	spin_unlock(&obj1->tab.lock);
	kt_cache_free(&tab.cache, obj1);

	kt_tab_destroy(&tab);

	pr_err("TSan: end of test.\n");
}

/* Other testing routines. */

static int current_thread_id(void)
{
	return current_thread_info()->task->pid;
}

static void kt_run_tests(void)
{
	pr_err("TSan: running tests, thread #%d.\n", current_thread_id());
	pr_err("TSan: objects in sync tab: %d.\n", kt_ctx.synctab.objnum);
	pr_err("\n");
	kt_test_hash_table();
	pr_err("\n");
	/*kt_test_race();
	pr_err("\n");
	kt_test_spinlock();
	pr_err("\n");*/
}

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
		kt_run_tests();

	return count;
}

static const struct file_operations kt_tests_operations = {
	.write = kt_tests_write,
};

void kt_tests_init(void)
{
	proc_create("ktsan_tests", S_IWUSR, NULL, &kt_tests_operations);
}
