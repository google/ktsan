#ifndef __X86_MM_TSAN_TSAN_H
#define __X86_MM_TSAN_TSAN_H

#define TSAN_SHADOW_SLOTS_LOG 2
#define TSAN_SHADOW_SLOTS (1 << TSAN_SHADOW_SLOTS_LOG)

#define TSAN_THREAD_ID_BITS     13
#define TSAN_EPOCH_BITS         42

struct shadow {
	unsigned long thread_id     : TSAN_THREAD_ID_BITS;
	unsigned long epoch         : TSAN_EPOCH_BITS;
	unsigned long access_offset : 3;
	unsigned long access_size   : 2;
	unsigned long is_read       : 1;
	unsigned long is_atomic     : 1;
	unsigned long is_freed      : 1;
};

/* Fow testing purposes. */
void tsan_access_memory(unsigned long addr, size_t size, bool is_read);

#endif /* __X86_MM_TSAN_TSAN_H */
