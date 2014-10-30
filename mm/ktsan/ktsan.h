#ifndef __X86_MM_KTSAN_KTSAN_H
#define __X86_MM_KTSAN_KTSAN_H

#include <linux/ktsan.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/percpu.h>
#include <linux/types.h>

#define KT_SHADOW_SLOTS_LOG 2
#define KT_SHADOW_SLOTS (1 << KT_SHADOW_SLOTS_LOG)

#define KT_GRAIN 8

#define KT_THREAD_ID_BITS 13
#define KT_CLOCK_BITS 42

#define KT_MAX_THREAD_ID 1024
#define KT_QUARANTINE_SIZE 512

#define KT_MAX_STACK_FRAMES 64

#define KT_COLLECT_STATS 1

#define KT_TRACE_PARTS 16
#define KT_TRACE_PART_SIZE (64 * 1024)
#define KT_TRACE_SIZE (KT_TRACE_PARTS * KT_TRACE_PART_SIZE)

/* Both arguments must be pointers. */
#define KT_ATOMIC_64_READ(ptr) \
	(atomic64_read((atomic64_t *)(ptr)))
#define KT_ATOMIC_64_SET(ptr, val) \
	(atomic64_set((atomic64_t *)(ptr), *(u64 *)(val)))
#define KT_ATOMIC_64_ADD(ptr, val) \
	(atomic64_add(*(u64 *)(val), (atomic64_t *)(ptr)))
/* TODO(xairy): use kt_atomic_*_no_ktsan when implemented. */

typedef unsigned long	uptr_t;
typedef unsigned long	kt_time_t;

typedef struct kt_thr_s			kt_thr_t;
typedef struct kt_clk_s			kt_clk_t;
typedef struct kt_tab_s			kt_tab_t;
typedef struct kt_tab_obj_s		kt_tab_obj_t;
typedef struct kt_tab_part_s		kt_tab_part_t;
typedef struct kt_tab_sync_s		kt_tab_sync_t;
typedef struct kt_tab_memblock_s	kt_tab_memblock_t;
typedef struct kt_tab_test_s		kt_tab_test_t;
typedef struct kt_ctx_s			kt_ctx_t;
typedef enum kt_stat_e			kt_stat_t;
typedef struct kt_stats_s		kt_stats_t;
typedef struct kt_cpu_s			kt_cpu_t;
typedef struct kt_race_info_s		kt_race_info_t;
typedef struct kt_cache_s		kt_cache_t;
typedef struct kt_stack_s		kt_stack_t;
typedef enum kt_event_type_e		kt_event_type_t;
typedef struct kt_event_s		kt_event_t;
typedef struct kt_part_header_s		kt_part_header_t;
typedef struct kt_trace_s		kt_trace_t;
typedef struct kt_id_manager_s		kt_id_manager_t;
typedef struct kt_thr_pool_s		kt_thr_pool_t;
typedef struct kt_shadow_s		kt_shadow_t;
typedef struct kt_percpu_sync_s		kt_percpu_sync_t;

/* Stack. */

struct kt_stack_s {
	unsigned int		pc[KT_MAX_STACK_FRAMES];
	int			size;
};

/* Trace. */

enum kt_event_type_e {
	kt_event_type_invalid,
	kt_event_type_func_enter,
	kt_event_type_func_exit,
	kt_event_type_lock,
	kt_event_type_unlock,
	kt_event_type_acquire,
	kt_event_type_release,
	kt_event_type_mop, /* memory operation */
	kt_event_type_thr_start,
	kt_event_type_thr_stop,
	kt_event_type_preempt_enable,
	kt_event_type_preempt_disable,
	kt_event_type_irq_enable,
	kt_event_type_irq_disable,
};

struct kt_event_s {
	unsigned int		type;
	unsigned int		pc;
};

struct kt_part_header_s {
	kt_stack_t		stack;
	kt_time_t		clock;
};

struct kt_trace_s {
	kt_part_header_t	headers[KT_TRACE_PARTS];
	kt_event_t		events[KT_TRACE_SIZE];
	unsigned long		position;
	spinlock_t		lock;
};

/* Clocks. */

struct kt_clk_s {
	kt_time_t		time[KT_MAX_THREAD_ID];
};

/* Shadow. */

struct kt_shadow_s {
	unsigned long tid	: KT_THREAD_ID_BITS;
	unsigned long clock	: KT_CLOCK_BITS;
	unsigned long offset	: 3;
	unsigned long size	: 2;
	unsigned long read	: 1;
};

/* Reports. */

struct kt_race_info_s {
	unsigned long		addr;
	kt_shadow_t		old;
	kt_shadow_t		new;
	unsigned long		strip_addr;
};

/* Internal allocator. */

struct kt_cache_s {
	unsigned long		base;
	unsigned long		mem_size;
	void			*head;
	spinlock_t		lock;
};

/* Hash table. */

struct kt_tab_obj_s {
	spinlock_t		lock;
	kt_tab_obj_t		*link;
	uptr_t			key;
};

struct kt_tab_part_s {
	spinlock_t		lock;
	kt_tab_obj_t		*head;
};

struct kt_tab_s {
	unsigned		size;
	unsigned		objsize;
	kt_tab_part_t		*parts;
	kt_cache_t		obj_cache;
	kt_cache_t		parts_cache;
};

struct kt_tab_sync_s {
	kt_tab_obj_t		tab;
	kt_clk_t		clk;
	kt_tab_sync_t		*next; /* next sync object in memblock */
	int			lock_tid; /* id of thread that locked mutex */
};

struct kt_tab_memblock_s {
	kt_tab_obj_t		tab;
	kt_tab_sync_t		*head;
};

struct kt_tab_test_s {
	kt_tab_obj_t tab;
	unsigned long data[4];
};

/* Threads. */

struct kt_thr_s {
	int			id;
	int			kid; /* kernel thread id */
	atomic_t		inside;	/* already inside of ktsan runtime */
	kt_cpu_t		*cpu;
	kt_clk_t		clk;
	kt_trace_t		trace;
	int			call_depth;
	struct list_head	quarantine_list;
	int			report_depth;
	int			preempt_depth;
	bool			irqs_disabled;
	struct list_head	percpu_list;
};

struct kt_thr_pool_s {
	kt_cache_t		cache;
	kt_thr_t		*thrs[KT_MAX_THREAD_ID];
	int			new_id;
	struct list_head	quarantine;
	int			quarantine_size;
	spinlock_t		lock;
};

/* Per-cpu synchronization. */

struct kt_percpu_sync_s {
	uptr_t addr;
	struct list_head list;
};

/* Statistics. */

enum kt_stat_e {
	kt_stat_access_read,
	kt_stat_access_write,
	kt_stat_access_size1,
	kt_stat_access_size2,
	kt_stat_access_size4,
	kt_stat_access_size8,
	kt_stat_sync_objects,
	kt_stat_sync_alloc,
	kt_stat_sync_free,
	kt_stat_memblock_objects,
	kt_stat_memblock_alloc,
	kt_stat_memblock_free,
	kt_stat_threads,
	kt_stat_thread_create,
	kt_stat_thread_destroy,
	kt_stat_count,
};

struct kt_stats_s {
	unsigned long		stat[kt_stat_count];
};

struct kt_cpu_s {
	kt_stats_t		stat;
};

/* Global. */

struct kt_ctx_s {
	int			enabled;
	kt_cpu_t __percpu	*cpus;
	kt_tab_t		sync_tab; /* sync addr -> sync object */
	kt_tab_t		memblock_tab; /* memory block -> sync objects */
	kt_tab_t		test_tab;
	kt_thr_pool_t		thr_pool;
	kt_cache_t		percpu_sync_cache;
};

extern kt_ctx_t kt_ctx;

/* Stack. */

static inline unsigned int kt_pc_compress(unsigned long pc)
{
	return (pc & UINT_MAX);
}

static inline unsigned long kt_pc_decompress(unsigned int pc)
{
	return ((ULONG_MAX - UINT_MAX) | pc);
}

void kt_stack_save_current(kt_stack_t *stack, unsigned long strip_addr);
void kt_stack_print(kt_stack_t *stack);
void kt_stack_print_current(unsigned long strip_addr);

/* Trace. */

void kt_trace_init(kt_trace_t *trace);
void kt_trace_add_event(kt_thr_t *thr, kt_event_type_t type, uptr_t addr);
void kt_trace_restore_stack(kt_thr_t *thr, kt_time_t clock, kt_stack_t *stack);
void kt_trace_dump(kt_trace_t *trace, unsigned long beg, unsigned long end);

/* Clocks. */

void kt_clk_init(kt_thr_t *thr, kt_clk_t *clk);
void kt_clk_acquire(kt_thr_t *thr, kt_clk_t *dst, kt_clk_t *src);

static inline
kt_time_t kt_clk_get(kt_clk_t *clk, int tid)
{
	WARN_ON_ONCE(tid >= KT_MAX_THREAD_ID);
	if (tid >= KT_MAX_THREAD_ID)
		return 0;
	return clk->time[tid];
}

static inline
void kt_clk_tick(kt_clk_t *clk, int tid)
{
	WARN_ON_ONCE(tid >= KT_MAX_THREAD_ID);
	if (tid >= KT_MAX_THREAD_ID)
		return;
	clk->time[tid]++;
}

/* Shadow. */

void *kt_shadow_get(uptr_t addr);
void kt_shadow_clear(uptr_t addr, size_t size);

/* Threads. */

void kt_thr_pool_init(void);

kt_thr_t *kt_thr_create(kt_thr_t *thr, int kid);
void kt_thr_destroy(kt_thr_t *thr, kt_thr_t *old);
kt_thr_t *kt_thr_get(int id);

void kt_thr_start(kt_thr_t *thr, uptr_t pc);
void kt_thr_stop(kt_thr_t *thr, uptr_t pc);

/* Synchronization. */

void kt_sync_acquire(kt_thr_t *thr, uptr_t pc, uptr_t addr);
void kt_sync_release(kt_thr_t *thr, uptr_t pc, uptr_t addr);

void kt_mtx_pre_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try);
void kt_mtx_post_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try);
void kt_mtx_pre_unlock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr);

int kt_atomic32_read(kt_thr_t *thr, uptr_t pc, uptr_t addr);
void kt_atomic32_set(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value);

void kt_atomic32_add(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value);
void kt_atomic32_sub(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value);
int kt_atomic32_sub_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value);
int kt_atomic32_add_negative(kt_thr_t *thr, uptr_t pc, uptr_t addr, int value);

void kt_atomic32_inc(kt_thr_t *thr, uptr_t pc, uptr_t addr);
void kt_atomic32_dec(kt_thr_t *thr, uptr_t pc, uptr_t addr);
int kt_atomic32_inc_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr);
int kt_atomic32_dec_and_test(kt_thr_t *thr, uptr_t pc, uptr_t addr);

int kt_atomic32_read_no_ktsan(const void *addr);
void kt_atomic32_set_no_ktsan(void *addr, int value);

void kt_atomic32_add_no_ktsan(void *addr, int value);
void kt_atomic32_sub_no_ktsan(void *addr, int value);
int kt_atomic32_sub_and_test_no_ktsan(void *addr, int value);
int kt_atomic32_add_negative_no_ktsan(void *addr, int value);

void kt_atomic32_inc_no_ktsan(void *addr);
void kt_atomic32_dec_no_ktsan(void *addr);
int kt_atomic32_inc_and_test_no_ktsan(void *addr);
int kt_atomic32_dec_and_test_no_ktsan(void *addr);

/* Per-cpu synchronization. */

void kt_preempt_add(kt_thr_t *thr, uptr_t pc, int value);
void kt_preempt_sub(kt_thr_t *thr, uptr_t pc, int value);

void kt_irq_disable(kt_thr_t *thr, uptr_t pc);
void kt_irq_enable(kt_thr_t *thr, uptr_t pc);
void kt_irq_save(kt_thr_t *thr, uptr_t pc);
void kt_irq_restore(kt_thr_t *thr, uptr_t pc, unsigned long flags);

void kt_percpu_acquire(kt_thr_t *thr, uptr_t pc, uptr_t addr);
void kt_percpu_release(kt_thr_t *thr, uptr_t pc);

/* Memory block allocation. */

uptr_t kt_memblock_addr(uptr_t addr);
void kt_memblock_alloc(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size);
void kt_memblock_free(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size);

/* Generic memory access. */

void kt_access(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size, bool read);
void kt_access_range(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t sz, bool rd);

void kt_access_imitate(kt_thr_t *thr, uptr_t pc, uptr_t addr,
				size_t size, bool read);
void kt_access_range_imitate(kt_thr_t *thr, uptr_t pc, uptr_t addr,
				size_t size, bool read);

/* Function tracing. */

void kt_func_entry(kt_thr_t *thr, uptr_t pc);
void kt_func_exit(kt_thr_t *thr);

/* Reports. */

void kt_report_disable(kt_thr_t *thr);
void kt_report_enable(kt_thr_t *thr);
void kt_report_race(kt_thr_t *thr, kt_race_info_t *info);

/* Internal allocator. */

void kt_cache_init(kt_cache_t *cache, size_t obj_size, size_t obj_max_num);
void kt_cache_destroy(kt_cache_t *cache);
void *kt_cache_alloc(kt_cache_t *cache);
void kt_cache_free(kt_cache_t *cache, void *obj);

/*
 * Hash table. Maps an address to an arbitrary object.
 * The object must start with kt_tab_obj_t.
 */

void kt_tab_init(kt_tab_t *tab, unsigned size,
		 unsigned obj_size, unsigned obj_max_num);
void kt_tab_destroy(kt_tab_t *tab);
void *kt_tab_access(kt_tab_t *tab, uptr_t key, bool *created, bool destroy);

/* Statistics. Enabled only when KT_COLLECT_STATS = 1. */

void kt_stat_init(void);

static inline void kt_stat_add(kt_thr_t *thr, kt_stat_t what, unsigned long x)
{
#if KT_COLLECT_STATS
	WARN_ON_ONCE(thr->cpu == NULL);
	if (thr->cpu == NULL)
		return;
	thr->cpu->stat.stat[what] += x;
#endif
}

static inline void kt_stat_inc(kt_thr_t *thr, kt_stat_t what)
{
	kt_stat_add(thr, what, 1);
}

static inline void kt_stat_dec(kt_thr_t *thr, kt_stat_t what)
{
	kt_stat_add(thr, what, -1);
}

/* Tests. */

void kt_tests_init(void);
void kt_tests_run_noinst(void);
void kt_tests_run_inst(void);
void kt_tests_run(void);

#endif /* __X86_MM_KTSAN_KTSAN_H */
