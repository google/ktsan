#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/sched.h>

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
