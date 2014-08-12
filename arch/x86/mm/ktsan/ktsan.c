#include "ktsan.h"

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

ktsan_ctx_t ktsan_ctx;

/* XXX: for debugging. */
#define REPEAT_N_AND_STOP(n) \
	static int scary_counter_##__LINE__ = 0; \
	if (++scary_counter_##__LINE__ < (n))

static int current_thread_id(void)
{
	return current_thread_info()->task->pid;
}

void ktsan_init(void)
{
	ktsan_ctx_t *ctx;
	ktsan_thr_t *thr;

	ctx = &ktsan_ctx;
	thr = &current->ktsan;
	BUG_ON(ctx->enabled);
	BUG_ON(thr->inside);
	thr->inside = true;

	ktsan_tab_init(&ctx->synctab, 10007, sizeof(ktsan_sync_t));

	thr->inside = false;
	ctx->enabled = 1;
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

static bool ranges_intersect(int first_offset, int first_size,
			     int second_offset, int second_size)
{
	if (first_offset + first_size < second_offset)
		return false;

	if (second_offset + second_size < first_offset)
		return false;

	return true;
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
		if (ktsan_clk_get(task->ktsan.clk, old.thread_id) >= old.clock) {
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

		return false;
	}

	/* Do the memory accesses intersect? */
	if (ranges_intersect(old.offset, (1 << old.size),
			     value.offset, (1 << value.size))) {
		if (old.thread_id == value.thread_id)
			return false;
		if (old.is_read && value.is_read)
			return false;

		/* TODO: compare clock. */

		info.addr = addr;
		info.old = old;
		info.new = value;
		info.strip_addr = _RET_IP_;
		report_race(&info);

		return false;
	}

	return false;
}

void ktsan_access_memory(unsigned long addr, size_t size, bool is_read)
{
	struct shadow value;
	unsigned long current_clock;
	struct task_struct *task;	
	struct shadow *slots;
	int i, thread_id;
	bool stored;

	task = current_thread_info()->task;	
	thread_id = task->pid;
	slots = map_memory_to_shadow(addr); /* FIXME: might be NULL */

	ktsan_clk_tick(task->ktsan.clk, thread_id);
	current_clock = ktsan_clk_get(task->ktsan.clk, thread_id);

	/* TODO: long accesses, size > 8. */

	/* TODO(xairy): log memory access. */

	value.thread_id = task->pid;
	value.clock = current_clock;
	value.offset = addr & ~KTSAN_GRAIN;

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

void ktsan_sync_acquire(void *addr)
{
	ktsan_thr_t *thr;

	thr = &current->ktsan;
	if (thr->inside)
		return;
	thr->inside = true;
	ktsan_acquire(thr, (uptr_t)_RET_IP_, (uptr_t)addr);
	thr->inside = false;
}
EXPORT_SYMBOL(ktsan_sync_acquire);

void ktsan_sync_release(void *addr)
{
	ktsan_thr_t *thr;

	thr = &current->ktsan;
	if (thr->inside)
		return;
	thr->inside = true;
	ktsan_release(thr, (uptr_t)_RET_IP_, (uptr_t)addr);
	thr->inside = false;
}
EXPORT_SYMBOL(ktsan_sync_release);
