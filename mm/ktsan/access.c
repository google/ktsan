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

	aligned_addr = round_down(addr, KT_GRAIN);
	shadow_offset = (aligned_addr & (PAGE_SIZE - 1)) * KT_SHADOW_SLOTS;
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

static bool update_one_shadow_slot(kt_thr_t *thr, uptr_t addr,
			struct shadow *slot, struct shadow value, bool stored)
{
	kt_race_info_t info;
	struct shadow old;

	KT_ATOMIC_64_SET(&old, slot);

	if (*(unsigned long *)(&old) == 0) {
		if (!stored) {
			KT_ATOMIC_64_SET(slot, &value);
			return true;
		}
		return false;
	}

	/* Is the memory access equal to the previous? */
	if (value.offset == old.offset && value.size == old.size) {
		/* Same thread? */
		if (value.tid == old.tid) {
			/* TODO. */
			return false;
		}

		/* Happens-before? */
		if (kt_clk_get(&thr->clk, old.tid) >= old.clock) {
			KT_ATOMIC_64_SET(slot, &value);
			return true;
		}

		if (old.read && value.read)
			return false;

		info.addr = addr;
		info.old = old;
		info.new = value;
		info.strip_addr = _RET_IP_;
		kt_report_race(&info);

		return false;
	}

	/* Do the memory accesses intersect? */
	if (ranges_intersect(old.offset, (1 << old.size),
			     value.offset, (1 << value.size))) {
		if (old.tid == value.tid)
			return false;
		if (old.read && value.read)
			return false;
		if (kt_clk_get(&thr->clk, old.tid) >= old.clock)
			return false;

		info.addr = addr;
		info.old = old;
		info.new = value;
		info.strip_addr = _RET_IP_;
		kt_report_race(&info);

		return false;
	}

	return false;
}

/*
   Size might be 0, 1, 2 or 3 and refers to the binary logarithm
   of the actual access size.
   Accessed region should fall into one 8-byte aligned region.
*/
void kt_access(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size, bool read)
{
	struct shadow value;
	unsigned long current_clock;
	struct shadow *slots;
	int i;
	bool stored;

	/* XXX: hack? */
	if ((addr & ~(KT_GRAIN - 1)) !=
	    ((addr + (1 << size) - 1) & ~(KT_GRAIN - 1)))
		size = 0;

	kt_stat_inc(thr, read ? kt_stat_access_read : kt_stat_access_write);
	kt_stat_inc(thr, kt_stat_access_size1 + size);

	slots = map_memory_to_shadow(addr);

	if (!slots) return; /* FIXME? */

	kt_clk_tick(&thr->clk, thr->id);
	current_clock = kt_clk_get(&thr->clk, thr->id);

	/* TODO(xairy): log memory access. */

	value.tid = thr->id;
	value.clock = current_clock;
	value.offset = addr & ~KT_GRAIN;
	value.size = size;
	value.read = read;

	stored = false;
	for (i = 0; i < KT_SHADOW_SLOTS; i++)
		stored |= update_one_shadow_slot(thr, addr, &slots[i],
						 value, stored);

	/*pr_err("thread: %d, addr: %lx, size: %u, read: %d, stored: %d\n",
		 (int)thr->id, addr, (int)size, (int)read, stored);*/

	if (!stored) {
		/* Evict random shadow slot. */
		KT_ATOMIC_64_SET(&slots[current_clock % KT_SHADOW_SLOTS],
				 &value);
	}
}

/* XXX: Relies the fact that log(KT_GRAIN) == 3. */
void kt_access_range(kt_thr_t *thr, uptr_t pc, uptr_t addr,
			size_t size, bool read)
{
	/* Handle unaligned beginning, if any. */
	for (; (addr & ~KT_GRAIN) && size; addr++, size--)
		kt_access(thr, pc, addr, 0, read);

	/* Handle middle part, if any. */
	for (; size >= KT_GRAIN; addr += KT_GRAIN, size -= KT_GRAIN)
		kt_access(thr, pc, addr, 3, read);

	/* Handle ending, if any. */
	for (; size; addr++, size--)
		kt_access(thr, pc, addr, 0, read);
}
