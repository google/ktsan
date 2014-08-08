#include <linux/tsan.h>

#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include <asm/bug.h>
#include <asm/page.h>
#include <asm/page_64.h>
#include <asm/thread_info.h>

#include "tsan.h"

static struct {
	int enabled;
} ctx = {
	.enabled = 0,
};

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

static bool physical_memory_addr(unsigned long addr)
{
	return (addr >= (unsigned long)(__va(0)) &&
		addr < (unsigned long)(__va(max_pfn << PAGE_SHIFT)));
}

static void *map_memory_to_shadow(unsigned long addr)
{
	struct page *page;
	unsigned long aligned_addr;
	unsigned long shadow_offset;

	if (!physical_memory_addr(addr))
		return NULL;

	/* XXX: kmemcheck checks something about pte here. */

	page = virt_to_page(addr);
	if (!page->shadow)
		return NULL;
	aligned_addr = round_down(addr, sizeof(unsigned long));
	shadow_offset = (aligned_addr & (PAGE_SIZE - 1)) * TSAN_SHADOW_SLOTS;
	return page->shadow + shadow_offset;
}

static void acquire(unsigned long *thread_vc, unsigned long *sync_vc)
{
	int i;

	for (i = 0; i < TSAN_MAX_THREAD_ID; i++)
		sync_vc[i] = max(thread_vc[i], sync_vc[i]);
}

static void release(unsigned long *thread_vc, unsigned long *sync_vc)
{
	int i;

	for (i = 0; i < TSAN_MAX_THREAD_ID; i++)
		thread_vc[i] = max(thread_vc[i], sync_vc[i]);
}

/*
static void store_release(unsigned long *thread_vc, unsigned long *sync_vc)
{
	int i;

	for (i = 0; i < TSAN_MAX_THREAD_ID; i++)
		sync_vc[i] = thread_vc[i];
}
*/

void tsan_enable(void)
{
	/* XXX: race on ctx.enabled? */
	// ctx.enabled = 0;
	ctx.enabled = 1;
}

void tsan_spin_lock_init(void *lock)
{
	spinlock_t *spin_lock = (spinlock_t *)lock;
	spin_lock->clock = NULL;
}
EXPORT_SYMBOL(tsan_spin_lock_init);

/* XXX: before actual lock or after? */
void tsan_spin_lock(void *lock)
{
	unsigned long addr = (unsigned long)lock;
	int thread_id = current_thread_id();
	spinlock_t *spin_lock = (spinlock_t *)lock;

	REPEAT_N_AND_STOP(20) TSAN_PRINT(
		"Thread #%d locked %lu.\n", thread_id, addr);

	//if (!ctx.enabled)
	//	return;

	if (!spin_lock->clock) {
		/*spin_lock->clock = kzalloc(
			sizeof(unsigned long) * TSAN_MAX_THREAD_ID, GFP_KERNEL);*/
		/*spin_lock->clock =
			(void *)__get_free_pages(GFP_KERNEL | __GFP_NOTRACK, 3);*/
	}
	//BUG_ON(!spin_lock->clock);
}
EXPORT_SYMBOL(tsan_spin_lock);

/* XXX: before actual unlock or after? */
void tsan_spin_unlock(void *lock)
{
	unsigned long addr = (unsigned long)lock;
	int thread_id = current_thread_id();
	REPEAT_N_AND_STOP(20) TSAN_PRINT(
		"Thread #%d unlocked %lu.\n", thread_id, addr);
}
EXPORT_SYMBOL(tsan_spin_unlock);

void tsan_thread_create(struct task_struct* task)
{
	memset(task->clock, 0, sizeof(task->clock));
}
EXPORT_SYMBOL(tsan_thread_create);


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
	BUG_ON(!shadow);

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

void tsan_access_memory(unsigned long addr, size_t size, bool is_read)
{
	
}
