/* ThreadSanitizer (TSan) is a tool that finds data race bugs. */

#ifndef LINUX_KTSAN_H
#define LINUX_KTSAN_H

#include <linux/types.h>

struct kmem_cache;
struct page;

#ifdef CONFIG_KTSAN

struct kt_thr_s;

struct ktsan_thr_s {
	struct kt_thr_s	*thr;
};

void ktsan_init_early(void);
void ktsan_init(void);

void ktsan_thr_create(struct ktsan_thr_s *new, int tid);
void ktsan_thr_finish(void);
void ktsan_thr_start(void);
void ktsan_thr_stop(void);

void ktsan_sync_acquire(void *addr);
void ktsan_sync_release(void *addr);

void ktsan_mtx_pre_lock(void *addr, bool write, bool try);
void ktsan_mtx_post_lock(void *addr, bool write, bool try);
void ktsan_mtx_pre_unlock(void *addr, bool write);

void ktsan_read1(void *addr);
void ktsan_read2(void *addr);
void ktsan_read4(void *addr);
void ktsan_read8(void *addr);
void ktsan_write1(void *addr);
void ktsan_write2(void *addr);
void ktsan_write4(void *addr);
void ktsan_write8(void *addr);

void ktsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node);
void ktsan_free_page(struct page *page, unsigned int order);
void ktsan_split_page(struct page *page, unsigned int order);

void ktsan_memblock_alloc(struct kmem_cache *cache, void *obj);
void ktsan_memblock_free(struct kmem_cache *cache, void *obj);

#else /* CONFIG_KTSAN */

/* When disabled TSan is no-op. */

struct ktsan_thr_s {
};

static inline void ktsan_init_early(void) {}
static inline void ktsan_init(void) {}

static inline void ktsan_thr_create(ktsan_thr_t *new, int tid) {}
static inline void ktsan_thr_finish(void) {}
static inline void ktsan_thr_start(void) {}
static inline void ktsan_thr_stop(void) {}

static inline void ktsan_sync_acquire(void *addr) {}
static inline void ktsan_sync_release(void *addr) {}

static inline void ktsan_mtx_pre_lock(void *addr, bool write) {}
static inline void ktsan_mtx_post_lock(void *addr, bool write) {}
static inline void ktsan_mtx_pre_unlock(void *addr, bool write) {}

static inline void ktsan_read1(void *addr) {}
static inline void ktsan_read2(void *addr) {}
static inline void ktsan_read4(void *addr) {}
static inline void ktsan_read8(void *addr) {}
static inline void ktsan_write1(void *addr) {}
static inline void ktsan_write2(void *addr) {}
static inline void ktsan_write4(void *addr) {}
static inline void ktsan_write8(void *addr) {}

static inline void ktsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node) {}
static inline void ktsan_free_page(struct page *page, unsigned int order) {}
static inline void ktsan_split_page(struct page *page, unsigned int order) {}

static inline void ktsan_memblock_alloc(struct kmem_cache *cache, void *obj) {}
static inline void ktsan_memblock_free(struct kmem_cache *cache, void *obj) {}

#endif /* CONFIG_KTSAN */

#endif /* LINUX_KTSAN_H */
