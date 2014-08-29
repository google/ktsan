#ifndef __X86_MM_KTSAN_KTSAN_H
#define __X86_MM_KTSAN_KTSAN_H

#include <linux/ktsan.h>
#include <linux/spinlock.h>
#include <linux/percpu.h>
#include <linux/types.h>

/* XXX: for debugging. */
void print_current_stack_trace(unsigned long strip_addr);

/* XXX: for debugging. */
#define KT_MGK(x, y) x ## y
#define KT_MGK2(x, y) KT_MGK(x, y)
#define REPEAT_N_AND_STOP(n) \
	static int KT_MGK2(scary_, __LINE__); if (++KT_MGK2(scary_, __LINE__) < (n))

#define KT_SHADOW_SLOTS_LOG 2
#define KT_SHADOW_SLOTS (1 << KT_SHADOW_SLOTS_LOG)

#define KT_GRAIN 8

#define KT_THREAD_ID_BITS     13
#define KT_CLOCK_BITS         42

#define KT_MAX_THREAD_ID 4096
#define KT_MAX_STACK_TRACE_FRAMES 64
#define KT_MAX_SYNC_PER_SLAB_OBJ 32

#define KT_COLLECT_STATS 1

/* Both arguments must be pointers. */
#define KT_ATOMIC_32_READ(ptr) \
	(atomic_read((atomic_t *)(ptr)))
#define KT_ATOMIC_32_SET(ptr, val) \
	(atomic_set((atomic_t *)(ptr), *(u32 *)(val)))
#define KT_ATOMIC_32_ADD(ptr, val) \
	(atomic_add(*(u32 *)(val), (atomic_t *)(ptr)))

/* Both arguments must be pointers. */
#define KT_ATOMIC_64_READ(ptr) \
	(atomic64_read((atomic64_t *)(ptr)))
#define KT_ATOMIC_64_SET(ptr, val) \
	(atomic64_set((atomic64_t *)(ptr), *(u64 *)(val)))
#define KT_ATOMIC_64_ADD(ptr, val) \
	(atomic64_add(*(u64 *)(val), (atomic64_t *)(ptr)))

typedef unsigned long	uptr_t;
typedef unsigned long	kt_time_t;

typedef struct kt_thr_s		kt_thr_t;
typedef struct kt_clk_s		kt_clk_t;
typedef struct kt_tab_s		kt_tab_t;
typedef struct kt_tab_obj_s	kt_tab_obj_t;
typedef struct kt_tab_part_s	kt_tab_part_t;
typedef struct kt_tab_sync_s	kt_tab_sync_t;
typedef struct kt_tab_slab_s	kt_tab_slab_t;
typedef struct kt_tab_test_s	kt_tab_test_t;
typedef struct kt_ctx_s		kt_ctx_t;
typedef enum kt_stat_e		kt_stat_t;
typedef struct kt_stats_s	kt_stats_t;
typedef struct kt_cpu_s		kt_cpu_t;
typedef struct kt_race_info_s	kt_race_info_t;
typedef struct kt_cache_s	kt_cache_t;

/* Clocks. */

struct kt_clk_s {
	kt_time_t time[KT_MAX_THREAD_ID];
};

/* Shadow. */

struct shadow {
	unsigned long tid	: KT_THREAD_ID_BITS;
	unsigned long clock	: KT_CLOCK_BITS;
	unsigned long offset	: 3;
	unsigned long size	: 2;
	unsigned long read	: 1;
};

struct kt_race_info_s {
	unsigned long		addr;
	struct shadow		old;
	struct shadow		new;
	unsigned long		strip_addr;
};

/* Internal allocator. */

struct kt_cache_s {
	unsigned long		addr;
	unsigned long		space;

	size_t			obj_size;
	int			obj_max_num;
	int			obj_num;

	int			head;
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
};

struct kt_tab_slab_s {
	kt_tab_obj_t		tab;
	uptr_t			syncs[KT_MAX_SYNC_PER_SLAB_OBJ];
	int 			sync_num;
};

struct kt_tab_test_s {
	kt_tab_obj_t tab;
	unsigned long data[4];
};

/* Stats. */

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
	kt_stat_slab_objects,
	kt_stat_slab_alloc,
	kt_stat_slab_free,
	kt_stat_count,
};

struct kt_stats_s {
	unsigned long		stat[kt_stat_count];
};

struct kt_cpu_s {
	kt_stats_t		stat;
};

struct kt_thr_s {
	unsigned		id;
	atomic_t		inside;	/* Already inside of ktsan runtime */
	kt_cpu_t		*cpu;
	kt_clk_t		clk;
};

/* Global. */

struct kt_ctx_s {
	int			enabled;
	kt_cpu_t __percpu	*cpus;
	kt_tab_t		sync_tab; /* sync addr -> sync object */
	kt_tab_t		slab_tab; /* memory block -> sync objects */
	kt_tab_t		test_tab;
};

extern kt_ctx_t kt_ctx;

/*
 * Statistics.  Enabled only when KT_COLLECT_STATS = 1.
 */
void kt_stat_init(void);

static inline unsigned long kt_stat_read(unsigned long *stat)
{
#if KT_COLLECT_STATS
	return KT_ATOMIC_64_READ(stat);
#else
	return 0;
#endif
}

static inline void kt_stat_add(unsigned long *stat, unsigned long x)
{
#if KT_COLLECT_STATS
	KT_ATOMIC_64_ADD(stat, &x);
#endif
}

static inline void kt_thr_stat_add(kt_thr_t *thr, kt_stat_t what,
				   unsigned long x)
{
	/* FIXME(xairy): thr->cpu might be NULL sometimes. */
	if (thr->cpu == NULL) {
		pr_err("TSan: WARNING: cpu for thread %d is NULL!\n", thr->id);
		print_current_stack_trace((u64)_RET_IP_);
		pr_err("\n");
		thr->cpu = this_cpu_ptr(kt_ctx.cpus);
	}
	kt_stat_add(&thr->cpu->stat.stat[what], x);
}

static inline void kt_thr_stat_inc(kt_thr_t *thr, kt_stat_t what)
{
	kt_thr_stat_add(thr, what, 1);
}

static inline void kt_thr_stat_dec(kt_thr_t *thr, kt_stat_t what)
{
	kt_thr_stat_add(thr, what, -1);
}

/*
 * Tests.
 */
void kt_tests_init(void);

/*
 * Threads.
 */
void kt_thr_create(kt_thr_t *thr, uptr_t pc, kt_thr_t *new, int tid);
void kt_thr_finish(kt_thr_t *thr, uptr_t pc);
void kt_thr_start(kt_thr_t *thr, uptr_t pc);
void kt_thr_stop(kt_thr_t *thr, uptr_t pc);

/*
 * Clocks.
 */
void kt_clk_init(kt_thr_t *thr, kt_clk_t *clk);
void kt_clk_destroy(kt_thr_t *thr, kt_clk_t *clk);
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

/*
 * Synchronization.
 */
void kt_sync_acquire(kt_thr_t *thr, uptr_t pc, uptr_t addr);
void kt_sync_release(kt_thr_t *thr, uptr_t pc, uptr_t addr);
void kt_mtx_pre_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try);
void kt_mtx_post_lock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr, bool try);
void kt_mtx_pre_unlock(kt_thr_t *thr, uptr_t pc, uptr_t addr, bool wr);

/*
 * Slab allocator.
 */
void kt_slab_alloc(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size);
void kt_slab_free(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size);

/*
 * Hash table. Maps an address to an arbitrary object.
 * The object must start with kt_tab_obj_t.
 */
void kt_tab_init(kt_tab_t *tab, unsigned size,
		 unsigned obj_size, unsigned obj_max_num);
void kt_tab_destroy(kt_tab_t *tab);
void *kt_tab_access(kt_tab_t *tab, uptr_t key, bool *created, bool destroy);

/*
 * Generic memory access.
 */
void kt_access(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size, bool read);
void kt_access_range(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t sz, bool rd);

/*
 * Reports.
 */
void kt_report_race(kt_race_info_t *info);

/*
 * Internal allocator.
 */
void kt_cache_init(kt_cache_t *cache, size_t obj_size, size_t obj_max_num);
void kt_cache_destroy(kt_cache_t *cache);
void *kt_cache_alloc(kt_cache_t *cache);
void kt_cache_free(kt_cache_t *cache, void *obj);

#endif /* __X86_MM_KTSAN_KTSAN_H */
