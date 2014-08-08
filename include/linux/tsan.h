/* ThreadSanitizer (TSan) is a tool that finds data race bugs. */

#ifndef LINUX_TSAN_H
#define LINUX_TSAN_H

#include <linux/spinlock_types.h>
#include <linux/types.h>

struct page;

#ifdef CONFIG_TSAN

#define TSAN_MAX_THREAD_ID 4096

void tsan_enable(void);

void tsan_spin_lock_init(void *lock);

void tsan_spin_lock(void *lock);
void tsan_spin_unlock(void *lock);

//void tsan_spin_read_lock(void *lock);
//void tsan_spin_read_unlock(void *lock);

//void tsan_spin_write_lock(void *lock);
//void tsan_spin_write_unlock(void *lock);

void tsan_thread_create(struct task_struct* task);

void tsan_thread_start(int thread_id, int cpu);
// void tsan_thread_stop(int thread_id, int cpu);

/* TODO(xairy): trylock? */

void tsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node);
void tsan_free_page(struct page *page, unsigned int order);
void tsan_split_page(struct page *page, unsigned int order);

#else /* CONFIG_TSAN */

/* When disabled TSAN is no-op. */

void tsan_enable(void) {}

void tsan_spin_lock_init(void *lock) {}

void tsan_spin_lock(void *lock) {}
void tsan_spin_unlock(void *lock) {}

//void tsan_spin_read_lock(void *lock) {}
//void tsan_spin_read_unlock(void *lock) {}

//void tsan_spin_write_lock(void *lock) {}
//void tsan_spin_write_unlock(void *lock) {}

void tsan_thread_create(struct task_struct* task) {}

void tsan_thread_start(int thread_id, int cpu) {}
//void tsan_thread_stop(int thread_id, int cpu) {}

void tsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node) {}
void tsan_free_page(struct page *page, unsigned int order) {}
void tsan_split_page(struct page *page, unsigned int order) {}

#endif /* CONFIG_TSAN */

#endif /* LINUX_TSAN_H */
