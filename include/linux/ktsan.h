/* ThreadSanitizer (TSan) is a tool that finds data race bugs. */

#ifndef LINUX_KTSAN_H
#define LINUX_KTSAN_H

#include <linux/spinlock_types.h>
#include <linux/types.h>

struct page;

#ifdef CONFIG_KTSAN

struct kt_thr_s;

struct ktsan_thr_s {
	struct kt_thr_s	*thr;
};

void ktsan_init_early(void);
void ktsan_init(void);

void ktsan_report_disable(void);
void ktsan_report_enable(void);

void ktsan_thr_create(struct ktsan_thr_s *new, int tid);
void ktsan_thr_destroy(struct ktsan_thr_s *old);
void ktsan_thr_start(void);
void ktsan_thr_stop(void);

void ktsan_memblock_alloc(void *addr, size_t size);
void ktsan_memblock_free(void *addr, size_t size);

void ktsan_sync_acquire(void *addr);
void ktsan_sync_release(void *addr);

void ktsan_mtx_pre_lock(void *addr, bool write, bool try);
void ktsan_mtx_post_lock(void *addr, bool write, bool try);
void ktsan_mtx_pre_unlock(void *addr, bool write);

void ktsan_rcu_read_lock(void);
void ktsan_rcu_read_unlock(void);
void ktsan_rcu_synchronize(void);
void ktsan_rcu_callback(void);

void ktsan_rcu_read_lock_bh(void);
void ktsan_rcu_read_unlock_bh(void);
void ktsan_rcu_synchronize_bh(void);
void ktsan_rcu_callback_bh(void);

void ktsan_rcu_read_lock_sched(void);
void ktsan_rcu_read_unlock_sched(void);
void ktsan_rcu_synchronize_sched(void);
void ktsan_rcu_callback_sched(void);

void ktsan_rcu_assign_pointer(void *new);
void ktsan_rcu_dereference(void *addr);

int ktsan_atomic32_read(const void *addr);
void ktsan_atomic32_set(void *addr, int value);

void ktsan_atomic32_add(void *addr, int value);
void ktsan_atomic32_sub(void *addr, int value);
int ktsan_atomic32_sub_and_test(void *addr, int value);
int ktsan_atomic32_add_negative(void *addr, int value);

void ktsan_atomic32_inc(void *addr);
void ktsan_atomic32_dec(void *addr);
int ktsan_atomic32_inc_and_test(void *addr);
int ktsan_atomic32_dec_and_test(void *addr);

long ktsan_atomic64_read(const void *addr);
void ktsan_atomic64_set(void *addr, long value);

void ktsan_atomic64_add(void *addr, long value);
void ktsan_atomic64_sub(void *addr, long value);
int ktsan_atomic64_sub_and_test(void *addr, long value);
int ktsan_atomic64_add_negative(void *addr, long value);

void ktsan_atomic64_inc(void *addr);
void ktsan_atomic64_dec(void *addr);
int ktsan_atomic64_inc_and_test(void *addr);
int ktsan_atomic64_dec_and_test(void *addr);

s64 ktsan_atomic64_xchg(void *addr, s64 value);
s32 ktsan_atomic32_xchg(void *addr, s32 value);

s64 ktsan_atomic64_cmpxchg(void *addr, s64 old, s64 new);
s32 ktsan_atomic32_cmpxchg(void *addr, s32 old, s32 new);
s16 ktsan_atomic16_cmpxchg(void *addr, s16 old, s16 new);

s64 ktsan_atomic64_xadd(void *addr, s64 value);
s32 ktsan_atomic32_xadd(void *addr, s32 value);
s16 ktsan_atomic16_xadd(void *addr, s16 value);

void ktsan_preempt_add(int value);
void ktsan_preempt_sub(int value);

void ktsan_irq_disable(void);
void ktsan_irq_enable(void);
void ktsan_irq_save(void);
void ktsan_irq_restore(unsigned long flags);

void ktsan_percpu_acquire(void *addr);

void ktsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node);
void ktsan_free_page(struct page *page, unsigned int order);
void ktsan_split_page(struct page *page, unsigned int order);

#else /* CONFIG_KTSAN */

/* When disabled ktsan is no-op. */

struct ktsan_thr_s {
};

static inline void ktsan_init_early(void) {}
static inline void ktsan_init(void) {}

static inline void ktsan_report_disable(void) {}
static inline void ktsan_report_enable(void) {}

static inline void ktsan_thr_create(struct ktsan_thr_s *new, int tid) {}
static inline void ktsan_thr_destroy(struct ktsan_thr_s *old) {}
static inline void ktsan_thr_start(void) {}
static inline void ktsan_thr_stop(void) {}

static inline void ktsan_memblock_alloc(void *addr, size_t size) {}
static inline void ktsan_memblock_free(void *addr, size_t size) {}

static inline void ktsan_sync_acquire(void *addr) {}
static inline void ktsan_sync_release(void *addr) {}

static inline void ktsan_mtx_pre_lock(void *addr, bool write, bool try) {}
static inline void ktsan_mtx_post_lock(void *addr, bool write, bool try) {}
static inline void ktsan_mtx_pre_unlock(void *addr, bool write) {}

static inline void ktsan_rcu_read_lock(void) {}
static inline void ktsan_rcu_read_unlock(void) {}
static inline void ktsan_rcu_synchronize(void) {}

/* ktsan_atomic* are not called in non-ktsan build. */

static inline void ktsan_preempt_disable(void) {}
static inline void ktsan_preempt_enable(void) {}

static inline void ktsan_irq_disable(void) {}
static inline void ktsan_irq_enable(void) {}

static inline void ktsan_percpu_acquire(void *addr) {}

static inline void ktsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node) {}
static inline void ktsan_free_page(struct page *page, unsigned int order) {}
static inline void ktsan_split_page(struct page *page, unsigned int order) {}

#endif /* CONFIG_KTSAN */

#endif /* LINUX_KTSAN_H */
