#include "ktsan.h"

#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/sched.h>

static inline
bool ranges_intersect(int first_offset, int first_size, int second_offset,
	int second_size)
{
	if (first_offset + first_size <= second_offset)
		return false;

	if (second_offset + second_size <= first_offset)
		return false;

	return true;
}

static inline
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
	old = *(kt_shadow_t*)&raw;

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

		info.addr = addr;
		info.old = old;
		info.new = value;
		/* Strip ktsan_* and kt_access frames. */
		info.strip_addr = (uptr_t)__builtin_return_address(1);
		kt_report_race(thr, &info);

		return false;
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

		info.addr = addr;
		info.old = old;
		info.new = value;
		/* Strip ktsan_* and kt_access frames. */
		info.strip_addr = (uptr_t)__builtin_return_address(1);
		kt_report_race(thr, &info);

		return false;
	}

	return false;
}

/*
   Size might be 0, 1, 2 or 3 and equals to the binary logarithm
   of the actual access size.
*/
void kt_access(kt_thr_t *thr, uptr_t pc, uptr_t addr, size_t size, bool read)
{
	kt_shadow_t value;
	unsigned long current_clock;
	kt_shadow_t *slots;
	int i;
	bool stored;

	kt_stat_inc(thr, read ? kt_stat_access_read : kt_stat_access_write);
	kt_stat_inc(thr, kt_stat_access_size1 + size);

	if (read && thr->read_disable_depth)
		return;

	slots = kt_shadow_get(addr);

	if (unlikely(!slots))
		return; /* FIXME? */

	current_clock = kt_clk_get(&thr->clk, thr->id);
	kt_trace_add_event(thr, kt_event_type_mop, kt_pc_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);

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

void kt_access_range(kt_thr_t *thr, uptr_t pc, uptr_t addr,
			size_t size, bool read)
{
	/* Handle unaligned beginning, if any. */
	for (; (addr & (KT_GRAIN - 1)) && size; addr++, size--)
		kt_access(thr, pc, addr, 0, read);

	/* Handle middle part, if any. */
	for (; size >= KT_GRAIN; addr += KT_GRAIN, size -= KT_GRAIN)
		kt_access(thr, pc, addr, 3, read);

	/* Handle ending, if any. */
	for (; size; addr++, size--)
		kt_access(thr, pc, addr, 0, read);
}

/*
   Size might be 0, 1, 2 or 3 and equals to the binary logarithm
   of the actual access size.
*/
void kt_access_imitate(kt_thr_t *thr, uptr_t pc, uptr_t addr,
				size_t size, bool read)
{
	kt_shadow_t value;
	kt_shadow_t *slots;
	int i;

	slots = kt_shadow_get(addr);
	if (!slots)
		return; /* FIXME? */

	kt_trace_add_event(thr, kt_event_type_mop, kt_pc_compress(pc));
	kt_clk_tick(&thr->clk, thr->id);

	value.tid = thr->id;
	value.clock = kt_clk_get(&thr->clk, thr->id);
	value.offset = addr & (KT_GRAIN - 1);
	value.size = size;
	value.read = read;

	for (i = 0; i < KT_SHADOW_SLOTS; i++)
		kt_atomic64_store_no_ktsan(&slots[i], KT_SHADOW_TO_LONG(value));
}

void kt_access_range_imitate(kt_thr_t *thr, uptr_t pc, uptr_t addr,
				size_t size, bool read)
{
	/* Handle unaligned beginning, if any. */
	for (; (addr & (KT_GRAIN - 1)) && size; addr++, size--)
		kt_access_imitate(thr, pc, addr, 0, read);

	/* Handle middle part, if any. */
	for (; size >= KT_GRAIN; addr += KT_GRAIN, size -= KT_GRAIN)
		kt_access_imitate(thr, pc, addr, 3, read);

	/* Handle ending, if any. */
	for (; size; addr++, size--)
		kt_access_imitate(thr, pc, addr, 0, read);
}
