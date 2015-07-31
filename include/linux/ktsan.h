/* ThreadSanitizer (TSan) is a tool that finds data race bugs. */

#ifndef LINUX_KTSAN_H
#define LINUX_KTSAN_H

#include <linux/types.h>

struct page;

enum ktsan_glob_sync_type_e {
	ktsan_glob_sync_type_rcu_common,
	ktsan_glob_sync_type_rcu_bh,
	ktsan_glob_sync_type_rcu_sched,
	ktsan_glob_sync_type_count,
};

extern int ktsan_glob_sync[ktsan_glob_sync_type_count];

#ifdef CONFIG_KTSAN

struct kt_thr_s;

struct ktsan_thr_s {
	struct kt_thr_s	*thr;
};

void ktsan_init_early(void);
void ktsan_init(void);

void ktsan_print_diagnostics(void);

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
s16 ktsan_atomic16_xchg(void *addr, s16 value);

s64 ktsan_atomic64_cmpxchg(void *addr, s64 old, s64 new);
s32 ktsan_atomic32_cmpxchg(void *addr, s32 old, s32 new);
s16 ktsan_atomic16_cmpxchg(void *addr, s16 old, s16 new);
s8 ktsan_atomic8_cmpxchg(void *addr, s8 old, s8 new);

s64 ktsan_atomic64_xadd(void *addr, s64 value);
s32 ktsan_atomic32_xadd(void *addr, s32 value);
s16 ktsan_atomic16_xadd(void *addr, s16 value);

void ktsan_bitop_set_bit(void *addr, long nr);
void ktsan_bitop_clear_bit(void *addr, long nr);
void ktsan_bitop_change_bit(void *addr, long nr);

int ktsan_bitop_test_and_set_bit(void *addr, long nr);
int ktsan_bitop_test_and_clear_bit(void *addr, long nr);
int ktsan_bitop_test_and_change_bit(void *addr, long nr);

void ktsan_membar_acquire(void);
void ktsan_membar_release(void);
void ktsan_membar_acq_rel(void);

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

/* ktsan_atomic* are not called in non-ktsan build. */
/* ktsan_bitop* are not called in non-ktsan build. */

static inline void ktsan_membar_acquire(void) {}
static inline void ktsan_membar_release(void) {}
static inline void ktsan_membar_acq_rel(void) {}

static inline void ktsan_preempt_add(int value) {}
static inline void ktsan_preempt_sub(int value) {}

static inline void ktsan_irq_disable(void) {}
static inline void ktsan_irq_enable(void) {}
static inline void ktsan_irq_save(void) {}
static inline void ktsan_irq_restore(unsigned long flags) {}

static inline void ktsan_percpu_acquire(void *addr) {}

static inline void ktsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node) {}
static inline void ktsan_free_page(struct page *page, unsigned int order) {}
static inline void ktsan_split_page(struct page *page, unsigned int order) {}

#endif /* CONFIG_KTSAN */

#endif /* LINUX_KTSAN_H */
