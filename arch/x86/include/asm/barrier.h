/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_BARRIER_H
#define _ASM_X86_BARRIER_H

#include <asm/alternative.h>
#include <asm/nops.h>

#include <linux/ktsan.h>

/*
 * Force strict CPU ordering.
 * And yes, this might be required on UP too when we're talking
 * to devices.
 */

#ifdef CONFIG_X86_32
#define mb() asm volatile(ALTERNATIVE("lock; addl $0,-4(%%esp)", "mfence", \
				      X86_FEATURE_XMM2) ::: "memory", "cc")
#define rmb() asm volatile(ALTERNATIVE("lock; addl $0,-4(%%esp)", "lfence", \
				       X86_FEATURE_XMM2) ::: "memory", "cc")
#define wmb() asm volatile(ALTERNATIVE("lock; addl $0,-4(%%esp)", "sfence", \
				       X86_FEATURE_XMM2) ::: "memory", "cc")
#else
#ifndef CONFIG_KTSAN
#define mb() 	asm volatile("mfence":::"memory")
#define rmb()	asm volatile("lfence":::"memory")
#define wmb()	asm volatile("sfence" ::: "memory")
#else /* CONFIG_KTSAN */
#define mb()	ktsan_thread_fence(ktsan_memory_order_acq_rel)
#define rmb()	ktsan_thread_fence(ktsan_memory_order_acquire)
#define wmb()	ktsan_thread_fence(ktsan_memory_order_release)
#endif
#endif

/**
 * array_index_mask_nospec() - generate a mask that is ~0UL when the
 * 	bounds check succeeds and 0 otherwise
 * @index: array element index
 * @size: number of elements in array
 *
 * Returns:
 *     0 - (index < size)
 */
static inline unsigned long array_index_mask_nospec(unsigned long index,
		unsigned long size)
{
	unsigned long mask;

	asm volatile ("cmp %1,%2; sbb %0,%0;"
			:"=r" (mask)
			:"g"(size),"r" (index)
			:"cc");
	return mask;
}

/* Override the default implementation from linux/nospec.h. */
#define array_index_mask_nospec array_index_mask_nospec

/* Prevent speculative execution past this barrier. */
#define barrier_nospec() alternative_2("", "mfence", X86_FEATURE_MFENCE_RDTSC, \
					   "lfence", X86_FEATURE_LFENCE_RDTSC)

#define dma_rmb()	barrier()
#define dma_wmb()	barrier()

#ifndef CONFIG_KTSAN
#ifdef CONFIG_X86_32
#define __smp_mb()	asm volatile("lock; addl $0,-4(%%esp)" ::: "memory", "cc")
#else
#define __smp_mb()	asm volatile("lock; addl $0,-4(%%rsp)" ::: "memory", "cc")
#endif
#define __smp_rmb()	dma_rmb()
#define __smp_wmb()	barrier()
#else /* CONFIG_KTSAN */
#define __smp_mb()	ktsan_thread_fence(ktsan_memory_order_acq_rel)
#define __smp_rmb()	ktsan_thread_fence(ktsan_memory_order_acquire)
#define __smp_wmb()	ktsan_thread_fence(ktsan_memory_order_release)
#endif
#define __smp_store_mb(var, value) do { (void)xchg(&var, value); } while (0)

#ifndef CONFIG_KTSAN
#define __smp_store_release(p, v)					\
do {									\
	compiletime_assert_atomic_type(*p);				\
	barrier();							\
	WRITE_ONCE(*p, v);						\
} while (0)

#define __smp_load_acquire(p)						\
({									\
	typeof(*p) ___p1 = READ_ONCE(*p);				\
	compiletime_assert_atomic_type(*p);				\
	barrier();							\
	___p1;								\
})
#else /* CONFIG_KTSAN */

#define __smp_store_release(p, v)                                                \
do {                                                                   \
	typeof(p) ___p1 = (p);                                          \
	typeof(v) ___v1 = (v);                                          \
	                                                               \
	compiletime_assert_atomic_type(*___p1);                         \
	                                                               \
	switch (sizeof(*___p1)) {                                       \
	case 1: ktsan_atomic8_store((void *)___p1, *((u8 *)&___v1), ktsan_memory_order_release); break; \
	case 2: ktsan_atomic16_store((void *)___p1, *((u16 *)&___v1), ktsan_memory_order_release); break;       \
	case 4: ktsan_atomic32_store((void *)___p1, *((u32 *)&___v1), ktsan_memory_order_release); break;       \
	case 8: ktsan_atomic64_store((void *)___p1, *((u64 *)&___v1), ktsan_memory_order_release); break;       \
	default: BUG(); break;                                          \
	}                                                               \
} while (0)

#define __smp_load_acquire(p)                                            \
({                                                                     \
	typeof(p) ___p1 = (p);                                          \
	typeof(*p) ___r;                                                \
	                                                               \
	compiletime_assert_atomic_type(*___p1);                         \
	                                                               \
	switch (sizeof(*___p1)) {                                       \
	case 1: *(u8 *)&___r = ktsan_atomic8_load((const void *)___p1, ktsan_memory_order_acquire); break;    \
	case 2: *(u16 *)&___r = ktsan_atomic16_load((const void *)___p1, ktsan_memory_order_acquire); break;  \
	case 4: *(u32 *)&___r = ktsan_atomic32_load((const void *)___p1, ktsan_memory_order_acquire); break;  \
	case 8: *(u64 *)&___r = ktsan_atomic64_load((const void *)___p1, ktsan_memory_order_acquire); break;  \
	default: BUG(); break;                                          \
	}                                                               \
	                                                               \
	___r;                                                           \
})

#endif /* CONFIG_KTSAN */

#ifndef CONFIG_KTSAN
/* Atomic operations are already serializing on x86 */
#define __smp_mb__before_atomic()	barrier()
#define __smp_mb__after_atomic()	barrier()
#else /* CONFIG_KTSAN */
#define __smp_mb__before_atomic()	ktsan_thread_fence(ktsan_memory_order_release)
#define __smp_mb__after_atomic()	ktsan_thread_fence(ktsan_memory_order_acquire)
#endif

#include <asm-generic/barrier.h>

#endif /* _ASM_X86_BARRIER_H */
