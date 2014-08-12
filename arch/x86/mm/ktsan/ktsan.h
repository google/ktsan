#ifndef __X86_MM_KTSAN_KTSAN_H
#define __X86_MM_KTSAN_KTSAN_H

#define KTSAN_SHADOW_SLOTS_LOG 2
#define KTSAN_SHADOW_SLOTS (1 << KTSAN_SHADOW_SLOTS_LOG)

#define KTSAN_GRAIN 8

#define KTSAN_THREAD_ID_BITS     13
#define KTSAN_CLOCK_BITS         42

struct shadow {
	unsigned long thread_id : KTSAN_THREAD_ID_BITS;
	unsigned long clock     : KTSAN_CLOCK_BITS;
	unsigned long offset    : 3;
	unsigned long size      : 2;
	unsigned long is_read   : 1;
	unsigned long is_atomic : 1;
	unsigned long is_freed  : 1;
};

/* Fow testing purposes. */
void ktsan_access_memory(unsigned long addr, size_t size, bool is_read);

#define KTSAN_MAX_STACK_TRACE_FRAMES 64

struct race_info {
	unsigned long addr;
	struct shadow old;
	struct shadow new;
	unsigned long strip_addr;
};

void report_race(struct race_info* info);

#endif /* __X86_MM_KTSAN_KTSAN_H */
