#include <linux/tsan.h>

#include <linux/gfp.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include <asm/thread_info.h>

#include "tsan.h"

/* XXX: for debugging. */
#define REPEAT_N_AND_STOP(n) \
	static int scary_counter_##__LINE__ = 0; \
	if (++scary_counter_##__LINE__ < (n))

/*
#if ASAN_COLORED_OUTPUT_ENABLE
	#define COLOR(x) (x)
#else
	#define COLOR(x) ""
#endif
*/
#define COLOR(x) (x)

#define COLOR_NORMAL  COLOR("\x1B[0m")
#define COLOR_RED     COLOR("\x1B[1;31m")
#define COLOR_GREEN   COLOR("\x1B[1;32m")
#define COLOR_YELLOW  COLOR("\x1B[1;33m")
#define COLOR_BLUE    COLOR("\x1B[1;34m")
#define COLOR_MAGENTA COLOR("\x1B[1;35m")
#define COLOR_WHITE   COLOR("\x1B[1;37m")

#define TSAN_PRINT(fmt, args...) \
	pr_err("%sTsan: %s"fmt, COLOR_GREEN, COLOR_NORMAL, ##args)

static int current_thread_id(void)
{
	return current_thread_info()->task->pid;
}

void tsan_spin_lock(spinlock_t *lock)
{
	unsigned long addr = (unsigned long)lock;
	int thread_id = current_thread_id();
	REPEAT_N_AND_STOP(20) TSAN_PRINT(
		"Thread #%d locked %lu.\n", thread_id, addr);
}
EXPORT_SYMBOL(tsan_spin_lock);

void tsan_spin_unlock(spinlock_t *lock)
{
	unsigned long addr = (unsigned long)lock;
	int thread_id = current_thread_id();
	REPEAT_N_AND_STOP(20) TSAN_PRINT(
		"Thread #%d unlocked %lu.\n", thread_id, addr);
}
EXPORT_SYMBOL(tsan_spin_unlock);

void tsan_thread_start(int thread_id, int cpu)
{
	REPEAT_N_AND_STOP(10) TSAN_PRINT(
		"Thread #%d started on cpu #%d.\n", thread_id, cpu);
}
EXPORT_SYMBOL(tsan_thread_start);

void tsan_thread_stop(int thread_id, int cpu)
{
}
EXPORT_SYMBOL(tsan_thread_stop);

void tsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node)
{
	struct page *shadow;
	int pages = 1 << order;
	int i;

	if (flags & (__GFP_HIGHMEM | __GFP_NOTRACK))
		return;

	shadow = alloc_pages_node(node, flags | __GFP_NOTRACK,
			order + TSAN_SHADOW_SLOTS_LOG);
	if (!shadow) {
		pr_err("TSan: failed to allocate shadow for page %p.\n", page);
		return;
	}

	for (i = 0; i < pages; i++)
		page[i].shadow = page_address(&shadow[i * TSAN_SHADOW_SLOTS]);
}
EXPORT_SYMBOL(tsan_alloc_page);

void tsan_free_page(struct page *page, unsigned int order)
{
	struct page *shadow;
	int pages = 1 << order;
	int i;

	if (!page[0].shadow)
		return;

	shadow = virt_to_page(page[0].shadow);

	for (i = 0; i < pages; i++)
		page[i].shadow = NULL;

	__free_pages(shadow, order);
}
EXPORT_SYMBOL(tsan_free_page);

void tsan_split_page(struct page *page, unsigned int order)
{
	struct page *shadow;

	if (!page[0].shadow)
		return;

	shadow = virt_to_page(page[0].shadow);
	split_page(shadow, order);
}
EXPORT_SYMBOL(tsan_split_page);
