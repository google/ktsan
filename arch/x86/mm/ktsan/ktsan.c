#include <linux/ktsan.h>

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

#include "ktsan.h"

static struct {
	int enabled;
} ctx = {
	.enabled = 0,
};

/* XXX: for debugging. */
#define REPEAT_N_AND_STOP(n) \
	static int scary_counter_##__LINE__ = 0; \
	if (++scary_counter_##__LINE__ < (n))

static int current_thread_id(void)
{
	return current_thread_info()->task->pid;
}

void ktsan_enable(void)

{
	/* XXX: race on ctx.enabled? */
	// ctx.enabled = 0;
	ctx.enabled = 1;
}

void ktsan_spin_lock_init(void *lock)
{
	spinlock_t *spin_lock = (spinlock_t *)lock;
	spin_lock->clock = NULL;
}
EXPORT_SYMBOL(ktsan_spin_lock_init);

/* XXX: before actual lock or after? */
void ktsan_spin_lock(void *lock)
{
	unsigned long addr = (unsigned long)lock;
	int thread_id = current_thread_id();
	spinlock_t *spin_lock = (spinlock_t *)lock;

	REPEAT_N_AND_STOP(20) pr_err(
		"TSan: Thread #%d locked %lu.\n", thread_id, addr);

	if (!spin_lock->clock) {
		/*spin_lock->clock = kzalloc(
			sizeof(unsigned long) * KTSAN_MAX_THREAD_ID, GFP_KERNEL);*/
		/*spin_lock->clock =
			(void *)__get_free_pages(GFP_KERNEL | __GFP_NOTRACK, 3);*/
	}
	//BUG_ON(!spin_lock->clock);
}
EXPORT_SYMBOL(ktsan_spin_lock);

/* XXX: before actual unlock or after? */
void ktsan_spin_unlock(void *lock)
{
	unsigned long addr = (unsigned long)lock;
	int thread_id = current_thread_id();
	REPEAT_N_AND_STOP(20) pr_err(
		"TSan: Thread #%d unlocked %lu.\n", thread_id, addr);
}
EXPORT_SYMBOL(ktsan_spin_unlock);

void ktsan_thread_create(struct task_struct* task)
{
	memset(task->clock, 0, sizeof(task->clock));
}
EXPORT_SYMBOL(ktsan_thread_create);


void ktsan_thread_start(int thread_id, int cpu)
{
	REPEAT_N_AND_STOP(10) pr_err(
		"TSan: Thread #%d started on cpu #%d.\n", thread_id, cpu);
}
EXPORT_SYMBOL(ktsan_thread_start);

void ktsan_thread_stop(int thread_id, int cpu)
{
}
EXPORT_SYMBOL(ktsan_thread_stop);

void ktsan_alloc_page(struct page *page, unsigned int order,
		     gfp_t flags, int node)
{
	struct page *shadow;
	int pages = 1 << order;
	int i;

	if (flags & (__GFP_HIGHMEM | __GFP_NOTRACK))
		return;

	shadow = alloc_pages_node(node, flags | __GFP_NOTRACK,
			order + KTSAN_SHADOW_SLOTS_LOG);
	BUG_ON(!shadow);

	memset(page_address(shadow), 0,
	       PAGE_SIZE * (1 << (order + KTSAN_SHADOW_SLOTS_LOG)));

	for (i = 0; i < pages; i++)
		page[i].shadow = page_address(&shadow[i * KTSAN_SHADOW_SLOTS]);
}
EXPORT_SYMBOL(ktsan_alloc_page);

void ktsan_free_page(struct page *page, unsigned int order)
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
EXPORT_SYMBOL(ktsan_free_page);

void ktsan_split_page(struct page *page, unsigned int order)
{
	struct page *shadow;

	if (!page[0].shadow)
		return;

	shadow = virt_to_page(page[0].shadow);
	split_page(shadow, order);
}
EXPORT_SYMBOL(ktsan_split_page);

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

	aligned_addr = round_down(addr, KTSAN_GRAIN);
	shadow_offset = (aligned_addr & (PAGE_SIZE - 1)) * KTSAN_SHADOW_SLOTS;
	return page->shadow + shadow_offset;
}

static bool update_one_shadow_slot(unsigned long addr, struct task_struct *task,
			struct shadow *slot, struct shadow value, bool stored)
{
	struct race_info info;
	struct shadow old = *slot; /* FIXME: atomic. */

	if (*(unsigned long *)(&old) == 0) {
		if (!stored) {
			*slot = value; /* FIXME: atomic. */
			return true;
		}
		return false;
	}

	/* Is the memory access equal to the previous? */
	if (value.offset == old.offset && value.size == old.size) {
		/* Same thread? */
		if (value.thread_id == old.thread_id) {
			/* TODO. */
			return false;
		}

		/* Happens-before? */
		if (task->clock[old.thread_id] >= old.clock) {
			*slot = value; /* FIXME: atomic. */
			return true;
		}

		if (old.is_read && value.is_read)
			return false;

		info.addr = addr;
		info.old = old;
		info.new = value;
		info.strip_addr = _RET_IP_;
		report_race(&info);
	}

	/* TODO: ranges intersection. */

	return false;
}

void ktsan_access_memory(unsigned long addr, size_t size, bool is_read)
{
	bool stored;
	int i;

	struct task_struct *task = current_thread_info()->task;	
	int thread_id = task->pid;
	struct shadow *slots = map_memory_to_shadow(addr); /* FIXME: might be NULL */
	unsigned long current_clock = ++task->clock[thread_id];

	/* TODO(xairy): log memory access. */

	struct shadow value;
	value.thread_id = task->pid;
	value.clock = current_clock;
	value.offset = addr & KTSAN_GRAIN;
	switch (size) {
	case 1:
		value.size = 0;
		break;
	case 2:
		value.size = 1;
		break;
	case 4:
		value.size = 2;
		break;
	case 8:
		value.size = 3;
		break;
	default:
		BUG_ON(true); /* FIXME: 16-byte accesses? */
	}
	value.is_read = is_read;
	value.is_atomic = 0; /* FIXME. */
	value.is_freed = 0; /* FIXME. */

	stored = false;
	for (i = 0; i < KTSAN_SHADOW_SLOTS; i++)
		stored |= update_one_shadow_slot(addr, task, &slots[i],
						 value, stored);

	if (!stored) {
		/* Evict random shadow slot. */
		slots[current_clock % KTSAN_SHADOW_SLOTS] = value; /* FIXME: atomic?*/
	}
}

void acquire(unsigned long *thread_vc, unsigned long *sync_vc)
{
	int i;

	for (i = 0; i < KTSAN_MAX_THREAD_ID; i++)
		sync_vc[i] = max(thread_vc[i], sync_vc[i]);
}

void release(unsigned long *thread_vc, unsigned long *sync_vc)
{
	int i;

	for (i = 0; i < KTSAN_MAX_THREAD_ID; i++)
		thread_vc[i] = max(thread_vc[i], sync_vc[i]);
}
