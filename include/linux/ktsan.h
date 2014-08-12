/* ThreadSanitizer (TSan) is a tool that finds data race bugs. */

#ifndef LINUX_KTSAN_H
#define LINUX_KTSAN_H

#include <linux/types.h>

typedef struct ktsan_thr_s ktsan_thr_t;

struct page;

#ifdef CONFIG_KTSAN

typedef struct ktsan_clk_s ktsan_clk_t;

struct ktsan_thr_s {
	ktsan_clk_t *clk;
};

#define KTSAN_MAX_THREAD_ID 4096
void ktsan_enable(void);

void ktsan_spin_lock_init(void *lock);

void ktsan_spin_lock(void *lock);
void ktsan_spin_unlock(void *lock);

//void ktsan_spin_read_lock(void *lock);
//void ktsan_spin_read_unlock(void *lock);

//void ktsan_spin_write_lock(void *lock);
//void ktsan_spin_write_unlock(void *lock);

void ktsan_thr_create(ktsan_thr_t *thr, ktsan_thr_t *parent);
void ktsan_thr_finish(ktsan_thr_t *thr);
void ktsan_thr_start(ktsan_thr_t *thr, int cpu);
void ktsan_thr_stop(ktsan_thr_t *thr, int cpu);

/* TODO(xairy): trylock? */

void ktsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node);
void ktsan_free_page(struct page *page, unsigned int order);
void ktsan_split_page(struct page *page, unsigned int order);

/* FIXME(xairy): for now. */
void ktsan_access_memory(unsigned long addr, size_t size, bool is_read);
void acquire(unsigned long *thread_vc, unsigned long *sync_vc);
void release(unsigned long *thread_vc, unsigned long *sync_vc);

#else /* CONFIG_KTSAN */

/* When disabled TSAN is no-op. */

struct ktsan_thr_s {
};

void ktsan_enable(void) {}

void ktsan_spin_lock_init(void *lock) {}

void ktsan_spin_lock(void *lock) {}
void ktsan_spin_unlock(void *lock) {}

//void ktsan_spin_read_lock(void *lock) {}
//void ktsan_spin_read_unlock(void *lock) {}

//void ktsan_spin_write_lock(void *lock) {}
//void ktsan_spin_write_unlock(void *lock) {}

static inline void ktsan_thr_create(ktsan_thr_t *thr, ktsan_thr_t *parent) {}
static inline void ktsan_thr_finish(ktsan_thr_t *thr) {}
static inline void ktsan_thr_start(ktsan_thr_t *thr, int cpu) {}
static inline void ktsan_thr_stop(ktsan_thr_t *thr, int cpu) {}

void ktsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node) {}
void ktsan_free_page(struct page *page, unsigned int order) {}
void ktsan_split_page(struct page *page, unsigned int order) {}

#endif /* CONFIG_KTSAN */

#endif /* LINUX_KTSAN_H */
