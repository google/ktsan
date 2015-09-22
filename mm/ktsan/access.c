#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/sched.h>

static __always_inline
bool ranges_intersect(int first_offset, int first_size, int second_offset,
	int second_size)
{
	if (first_offset + first_size <= second_offset)
		return false;

	if (second_offset + second_size <= first_offset)
		return false;

	return true;
}

static __always_inline
bool update_one_shadow_slot(kt_thr_t *thr, uptr_t addr, kt_shadow_t *slot,
	kt_shadow_t value, bool stored)
{
	kt_race_info_t info;
	kt_shadow_t old;
	u64 raw;

	raw = kt_atomic64_load_no_ktsan(slot);
	if (raw == 0) {
		if (!stored)
			kt_atomic64_store_no_ktsan(slot,
				KT_SHADOW_TO_LONG(value));
		return true;
	}
	old = *(kt_shadow_t *)&raw;

	/* Is the memory access equal to the previous? */
	if (value.offset == old.offset && value.size == old.size) {
		/* Same thread? */
		if (likely(value.tid == old.tid)) {
			/* TODO. */
			return false;
		}

		/* Happens-before? */
		if (likely(kt_clk_get(&thr->clk, old.tid) >= old.clock)) {
			if (!stored)
				kt_atomic64_store_no_ktsan(slot,
					KT_SHADOW_TO_LONG(value));
			return true;
		}

		if (likely(old.read && value.read))
			return false;

		goto report_race;
	}

	/* Do the memory accesses intersect? */
	if (unlikely(ranges_intersect(old.offset, (1 << old.size),
			     value.offset, (1 << value.size)))) {
		if (old.tid == value.tid)
			return false;
		if (old.read && value.read)
			return false;
		if (kt_clk_get(&thr->clk, old.tid) >= old.clock)
			return false;

report_race:
		info.addr = addr;
		info.old = old;
		info.new = value;
		kt_report_race(thr, &info);

		return true;
	}

	return false;
}

static __always_inline
void kt_access_impl(kt_thr_t *thr, kt_shadow_t *slots, kt_time_t current_clock,
		uptr_t addr, size_t size, bool read)
{
	kt_shadow_t value;
	int i;
	bool stored;

	kt_stat_inc(read ? kt_stat_access_read : kt_stat_access_write);
	kt_stat_inc(kt_stat_access_size1 + size);

	value.tid = thr->id;
	value.clock = current_clock;
	value.offset = addr & (KT_GRAIN - 1);
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
		kt_atomic64_store_no_ktsan(
			&slots[current_clock % KT_SHADOW_SLOTS],
			KT_SHADOW_TO_LONG(value));
	}
}

/*
   Size might be 0, 1, 2 or 3 and equals to the binary logarithm
   of the actual access size.
*/
void kt_access(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size, bool read)
{
	kt_time_t current_clock;
	kt_shadow_t *slots;

	if (read && thr->read_disable_depth)
		return;

	slots = kt_shadow_get(addr);
	if (unlikely(!slots))
		return; /* FIXME? */

	current_clock = kt_clk_get(&thr->clk, thr->id);
	kt_trace_add_event(thr, kt_event_mop, kt_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);

	kt_access_impl(thr, slots, current_clock, addr, size, read);
}

void kt_access_range(kt_thr_t *thr, uptr_t pc, uptr_t addr,
			size_t size, bool read)
{
	kt_time_t current_clock;
	kt_shadow_t *slots;

	BUG_ON(size == 0);
	if (read && thr->read_disable_depth)
		return;

	slots = kt_shadow_get(addr);
	if (unlikely(!slots))
		return; /* FIXME? */

	current_clock = kt_clk_get(&thr->clk, thr->id);
	kt_trace_add_event(thr, kt_event_mop, kt_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);

	/* Handle unaligned beginning, if any. */
	if (addr & (KT_GRAIN - 1)) {
		for (; (addr & (KT_GRAIN - 1)) && size; addr++, size--)
			kt_access_impl(thr, slots, current_clock, addr,
				KT_ACCESS_SIZE_1, read);
		slots += KT_SHADOW_SLOTS;
	}

	/* Handle middle part, if any. */
	for (; size >= KT_GRAIN; addr += KT_GRAIN, size -= KT_GRAIN) {
		kt_access_impl(thr, slots, current_clock, addr,
			KT_ACCESS_SIZE_8, read);
		slots += KT_SHADOW_SLOTS;
	}

	/* Handle ending, if any. */
	for (; size; addr++, size--)
		kt_access_impl(thr, slots, current_clock, addr,
			KT_ACCESS_SIZE_1, read);
}

void kt_access_range_imitate(kt_thr_t *thr, uptr_t pc, uptr_t addr,
				size_t size, bool read)
{
	kt_shadow_t value;
	kt_shadow_t *slots;
	int i;

	/* Currently it is called only from kt_memblock_alloc, so the address
	 * and size must be multiple of KT_GRAIN. */
	BUG_ON((addr & (KT_GRAIN - 1)) != 0);
	BUG_ON((size & (KT_GRAIN - 1)) != 0);

	slots = kt_shadow_get(addr);
	if (!slots)
		return; /* FIXME? */

	kt_trace_add_event(thr, kt_event_mop, kt_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);

	/* Below we assume that access size 8 covers whole grain. */
	BUG_ON(KT_GRAIN != (1 << KT_ACCESS_SIZE_8));

	value.tid = thr->id;
	value.clock = kt_clk_get(&thr->clk, thr->id);
	value.offset = 0;
	value.size = KT_ACCESS_SIZE_8;
	value.read = read;

	for (; size; size -= KT_GRAIN) {
		for (i = 0; i < KT_SHADOW_SLOTS; i++, slots++)
			kt_atomic64_store_no_ktsan(slots,
				i ? 0 : KT_SHADOW_TO_LONG(value));
	}
}
