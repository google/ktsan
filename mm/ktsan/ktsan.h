#ifndef __X86_MM_KTSAN_KTSAN_H
#define __X86_MM_KTSAN_KTSAN_H

#include <linux/ktsan.h>
#include <linux/spinlock.h>
#include <linux/percpu.h>
#include <linux/types.h>

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

#define KT_MAX_THREAD_ID 1024
#define KT_MAX_STACK_FRAMES 64

#define KT_COLLECT_STATS 1

#define KT_TRACE_PARTS 64
#define KT_TRACE_PART_SIZE (2 * 1024)
#define KT_TRACE_SIZE (KT_TRACE_PARTS * KT_TRACE_PART_SIZE)

/* Both arguments must be pointers. */
#define KT_ATOMIC_64_READ(ptr) \
	(atomic64_read((atomic64_t *)(ptr)))
#define KT_ATOMIC_64_SET(ptr, val) \
	(atomic64_set((atomic64_t *)(ptr), *(u64 *)(val)))
#define KT_ATOMIC_64_ADD(ptr, val) \
	(atomic64_add(*(u64 *)(val), (atomic64_t *)(ptr)))
/* TODO(xairy): use kt_atomic_pure_* when implemented. */

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

/* Stack. */

struct kt_stack_s {
	unsigned int		pc[KT_MAX_STACK_FRAMES];
	int			size;
};

/* Trace. */

enum kt_event_type_e {
	kt_event_type_func_enter,
	kt_event_type_func_exit,
	kt_event_type_lock,
	kt_event_type_unlock,
};

struct kt_event_s {
	unsigned int		type;
	unsigned int		pc;
};

struct kt_part_header_s {
	kt_stack_t		stack;	
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

struct shadow {
	unsigned long tid	: KT_THREAD_ID_BITS;
	unsigned long clock	: KT_CLOCK_BITS;
	unsigned long offset	: 3;
	unsigned long size	: 2;
	unsigned long read	: 1;
};

/* Reports. */

struct kt_race_info_s {
	unsigned long		addr;
	struct shadow		old;
	struct shadow		new;
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
};

struct kt_tab_memblock_s {
	kt_tab_obj_t		tab;
	kt_tab_sync_t		*head;
};

struct kt_tab_test_s {
	kt_tab_obj_t tab;
	unsigned long data[4];
};

/* Ids. */

struct kt_id_manager_s {
	int			ids[KT_MAX_THREAD_ID];
	int			head;
	spinlock_t		lock;
};

/* Threads. */

struct kt_thr_s {
	int			kid; /* kernel thread id */
	int			id;
	atomic_t		inside;	/* Already inside of ktsan runtime */
	kt_cpu_t		*cpu;
	kt_clk_t		clk;
	kt_trace_t		trace;
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
	kt_cache_t		thr_cache;
	kt_id_manager_t		thr_id_manager;
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

void kt_stack_print_current(unsigned long strip_addr);

/* Clocks. */

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

/* Ids. */

void kt_id_init(kt_id_manager_t *mgr);
int kt_id_new(kt_id_manager_t *mgr);
void kt_id_free(kt_id_manager_t *mgr, int id);

/* Threads. */

void kt_thr_create(kt_thr_t *thr, uptr_t pc, kt_thr_t *new, int tid);
void kt_thr_destroy(kt_thr_t *thr, uptr_t pc, kt_thr_t *old);
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

int kt_atomic32_pure_read(const void *addr);
void kt_atomic32_pure_set(void *addr, int value);

/* Memory block allocation. */

void kt_memblock_alloc(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size);
void kt_memblock_free(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size);

/* Generic memory access. */

void kt_access(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size, bool read);
void kt_access_range(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t sz, bool rd);

/* Reports. */

void kt_report_race(kt_race_info_t *info);

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
	/* FIXME(xairy): thr->cpu might be NULL sometimes. */
	if (thr->cpu == NULL) {
		pr_err("TSan: WARNING: cpu for thread %d is NULL!\n", thr->id);
		/*kt_print_current_stack_trace((u64)_RET_IP_);*/
		pr_err("\n");
		thr->cpu = this_cpu_ptr(kt_ctx.cpus);
	}
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

#endif /* __X86_MM_KTSAN_KTSAN_H */
