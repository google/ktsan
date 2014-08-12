/* ThreadSanitizer (TSan) is a tool that finds data race bugs. */

#ifndef LINUX_KTSAN_H
#define LINUX_KTSAN_H

#include <linux/types.h>

typedef struct ktsan_thr_s ktsan_thr_t;

struct page;

#ifdef CONFIG_KTSAN

#define KTSAN_MAX_THREAD_ID 4096

typedef struct ktsan_clk_s ktsan_clk_t;

struct ktsan_thr_s {
	bool		inside;
	ktsan_clk_t	*clk;
};

void ktsan_init(void);

void ktsan_thr_create(ktsan_thr_t *thr, ktsan_thr_t *parent);
void ktsan_thr_finish(ktsan_thr_t *thr);
void ktsan_thr_start(ktsan_thr_t *thr, int cpu);
void ktsan_thr_stop(ktsan_thr_t *thr, int cpu);

void ktsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node);
void ktsan_free_page(struct page *page, unsigned int order);
void ktsan_split_page(struct page *page, unsigned int order);

/* FIXME(xairy): for now. */
void ktsan_access_memory(unsigned long addr, size_t size, bool is_read);

void ktsan_sync_acquire(void *addr);
void ktsan_sync_release(void *addr);

void ktsan_mtx_pre_lock(void *addr, bool write, bool try);
void ktsan_mtx_post_lock(void *addr, bool write, bool try);
void ktsan_mtx_pre_unlock(void *addr, bool write);

#else /* CONFIG_KTSAN */

/* When disabled TSAN is no-op. */

struct ktsan_thr_s {
};

static inline void ktsan_init(void) {}

static inline void ktsan_thr_create(ktsan_thr_t *thr, ktsan_thr_t *parent) {}
static inline void ktsan_thr_finish(ktsan_thr_t *thr) {}
static inline void ktsan_thr_start(ktsan_thr_t *thr, int cpu) {}
static inline void ktsan_thr_stop(ktsan_thr_t *thr, int cpu) {}

static inline void ktsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node) {}
static inline void ktsan_free_page(struct page *page, unsigned int order) {}
static inline void ktsan_split_page(struct page *page, unsigned int order) {}

static inline void ktsan_sync_acquire(void *addr) {}
static inline void ktsan_sync_release(void *addr) {}

static inline void ktsan_mtx_pre_lock(void *addr, bool write) {}
static inline void ktsan_mtx_post_lock(void *addr, bool write) {}
static inline void ktsan_mtx_pre_unlock(void *addr, bool write) {}

#endif /* CONFIG_KTSAN */

#endif /* LINUX_KTSAN_H */
