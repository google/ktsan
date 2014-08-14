#ifndef __X86_MM_KTSAN_KTSAN_H
#define __X86_MM_KTSAN_KTSAN_H

#include <linux/ktsan.h>
#include <linux/spinlock.h>
#include <linux/percpu.h>

#define KT_SHADOW_SLOTS_LOG 2
#define KT_SHADOW_SLOTS (1 << KT_SHADOW_SLOTS_LOG)

#define KT_GRAIN 8

#define KT_THREAD_ID_BITS     13
#define KT_CLOCK_BITS         42

#define KT_MAX_THREAD_ID 4096
#define KT_MAX_STACK_TRACE_FRAMES 64

#define KT_COLLECT_STATS 1

typedef unsigned long uptr_t;
typedef unsigned long kt_time_t;

typedef struct kt_thr_s		kt_thr_t;
typedef struct kt_clk_s		kt_clk_t;
typedef struct kt_tab_s		kt_tab_t;
typedef struct kt_tab_obj_s	kt_tab_obj_t;
typedef struct kt_tab_part_s	kt_tab_part_t;
typedef struct kt_sync_s	kt_sync_t;
typedef struct kt_ctx_s		kt_ctx_t;
typedef enum kt_stat_e		kt_stat_t;
typedef struct kt_stats_s	kt_stats_t;
typedef struct kt_cpu_s		kt_cpu_t;
typedef struct kt_race_info_s	kt_race_info_t;

struct kt_clk_s {
	kt_time_t time[KT_MAX_THREAD_ID];
};

struct shadow {
	unsigned long tid	: KT_THREAD_ID_BITS;
	unsigned long clock	: KT_CLOCK_BITS;
	unsigned long offset	: 3;
	unsigned long size	: 2;
	unsigned long read	: 1;
};

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
};

struct kt_sync_s {
	kt_tab_obj_t		tab;
	kt_clk_t		clk;
};

struct kt_race_info_s {
	unsigned long 		addr;
	struct shadow 		old;
	struct shadow 		new;
	unsigned long 		strip_addr;
};

enum kt_stat_e {
	kt_stat_access_read,
	kt_stat_access_write,
	kt_stat_access_size1,
	kt_stat_access_size2,
	kt_stat_access_size4,
	kt_stat_access_size8,
	kt_stat_count,
};

struct kt_stats_s {
	unsigned long long	stat[kt_stat_count];
};

struct kt_cpu_s {
	kt_stats_t		stat;
};

struct kt_thr_s {
	unsigned		id;
	bool			inside;	/* Already inside of ktsan runtime */
	kt_cpu_t		*cpu;
	kt_clk_t		clk;
};

struct kt_ctx_s {
	int			enabled;
	kt_cpu_t __percpu	*cpus;
	kt_tab_t		synctab;
};

extern kt_ctx_t kt_ctx;

/*
 * Statistics.  Enabled only when KT_COLLECT_STATS = 1.
 */
void kt_stat_init(void);

static inline void kt_stat_add(kt_thr_t *thr, kt_stat_t what, unsigned long x)
{
#if KT_COLLECT_STATS
	/* TODO(dvyukov): remove this when we have cpu in all events */
	if (thr->cpu == NULL)
		return;
	thr->cpu->stat.stat[what] += x;
#endif
}

static inline void kt_stat_inc(kt_thr_t *thr, kt_stat_t what)
{
	kt_stat_add(thr, what, 1);
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
	return clk->time[tid];
}

static inline
void kt_clk_tick(kt_clk_t *clk, int tid)
{
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
void kt_tab_init(kt_tab_t *tab, unsigned size, unsigned objsize);
void kt_tab_destroy(kt_tab_t *tab);
void *kt_tab_access(kt_tab_t *tab, uptr_t key, bool *created, bool destroy);

/*
 * Generic memory access.
 */
void kt_access(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size, bool read);

/*
 * Reports.
 */
void kt_report_race(kt_race_info_t *info);
#endif /* __X86_MM_KTSAN_KTSAN_H */
