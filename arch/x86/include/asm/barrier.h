#ifndef _ASM_X86_BARRIER_H
#define _ASM_X86_BARRIER_H

#include <asm/alternative.h>
#include <asm/nops.h>

#include <linux/ktsan.h>

/*
 * Force strict CPU ordering.
 * And yes, this is required on UP too when we're talking
 * to devices.
 */

#ifdef CONFIG_X86_32
/*
 * Some non-Intel clones support out of order store. wmb() ceases to be a
 * nop for these.
 */
#define mb() alternative("lock; addl $0,0(%%esp)", "mfence", X86_FEATURE_XMM2)
#define rmb() alternative("lock; addl $0,0(%%esp)", "lfence", X86_FEATURE_XMM2)
#define wmb() alternative("lock; addl $0,0(%%esp)", "sfence", X86_FEATURE_XMM)
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

#ifdef CONFIG_X86_PPRO_FENCE
#define dma_rmb()	rmb()
#else
#define dma_rmb()	barrier()
#endif
#define dma_wmb()	barrier()

#ifdef CONFIG_SMP
#ifndef CONFIG_KTSAN
#define smp_mb()	mb()
#define smp_rmb()	dma_rmb()
#define smp_wmb()	barrier()
#else /* CONFIG_KTSAN */
#define smp_mb()	ktsan_thread_fence(ktsan_memory_order_acq_rel)
#define smp_rmb()	ktsan_thread_fence(ktsan_memory_order_acquire)
#define smp_wmb()	ktsan_thread_fence(ktsan_memory_order_release)
#endif
#define smp_store_mb(var, value) do { (void)xchg(&var, value); } while (0)
#else /* !SMP */
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#define smp_store_mb(var, value) do { WRITE_ONCE(var, value); barrier(); } while (0)
#endif /* SMP */

#ifndef CONFIG_KTSAN
#define read_barrier_depends()		do { } while (0)
#define smp_read_barrier_depends()	do { } while (0)
#else /* CONFIG_KTSAN */
#define read_barrier_depends()		\
	ktsan_thread_fence(ktsan_memory_order_acquire)
#define smp_read_barrier_depends()	\
	ktsan_thread_fence(ktsan_memory_order_acquire)
#endif

#ifndef CONFIG_KTSAN

#if defined(CONFIG_X86_PPRO_FENCE)

/*
 * For this option x86 doesn't have a strong TSO memory
 * model and we should fall back to full barriers.
 */

#define smp_store_release(p, v)						\
do {									\
	compiletime_assert_atomic_type(*p);				\
	smp_mb();							\
	WRITE_ONCE(*p, v);						\
} while (0)

#define smp_load_acquire(p)						\
({									\
	typeof(*p) ___p1 = READ_ONCE(*p);				\
	compiletime_assert_atomic_type(*p);				\
	smp_mb();							\
	___p1;								\
})

#else /* regular x86 TSO memory ordering */

#define smp_store_release(p, v)						\
do {									\
	compiletime_assert_atomic_type(*p);				\
	barrier();							\
	WRITE_ONCE(*p, v);						\
} while (0)

#define smp_load_acquire(p)						\
({									\
	typeof(*p) ___p1 = READ_ONCE(*p);				\
	compiletime_assert_atomic_type(*p);				\
	barrier();							\
	___p1;								\
})

#endif

#else /* CONFIG_KTSAN */

#define smp_store_release(p, v)						\
do {									\
	typeof(p) ___p1 = (p);						\
	typeof(v) ___v1 = (v);						\
									\
	compiletime_assert_atomic_type(*___p1);				\
									\
	switch (sizeof(*___p1)) {					\
	case 1: ktsan_atomic8_store(___p1, *((u8 *)&___v1), ktsan_memory_order_release); break;	\
	case 2: ktsan_atomic16_store(___p1, *((u16 *)&___v1), ktsan_memory_order_release); break;	\
	case 4: ktsan_atomic32_store(___p1, *((u32 *)&___v1), ktsan_memory_order_release); break;	\
	case 8: ktsan_atomic64_store(___p1, *((u64 *)&___v1), ktsan_memory_order_release); break;	\
	default: BUG(); break;						\
	}								\
} while (0)

#define smp_load_acquire(p)						\
({									\
	typeof(p) ___p1 = (p);						\
	typeof(*p) ___r;						\
									\
	compiletime_assert_atomic_type(*___p1);				\
									\
	switch (sizeof(*___p1)) {					\
	case 1: *(u8 *)&___r = ktsan_atomic8_load(___p1, ktsan_memory_order_acquire); break;	\
	case 2: *(u16 *)&___r = ktsan_atomic16_load(___p1, ktsan_memory_order_acquire); break;	\
	case 4: *(u32 *)&___r = ktsan_atomic32_load(___p1, ktsan_memory_order_acquire); break;	\
	case 8: *(u64 *)&___r = ktsan_atomic64_load(___p1, ktsan_memory_order_acquire); break;	\
	default: BUG(); break;						\
	}								\
									\
	___r;								\
})

#endif /* CONFIG_KTSAN */

/* Atomic operations are already serializing on x86 */
#ifndef CONFIG_KTSAN
#define smp_mb__before_atomic()	barrier()
#define smp_mb__after_atomic()	barrier()
#else /* CONFIG_KTSAN */
#define smp_mb__before_atomic()	ktsan_thread_fence(ktsan_memory_order_release)
#define smp_mb__after_atomic()	ktsan_thread_fence(ktsan_memory_order_acquire)
#endif

/*
 * Stop RDTSC speculation. This is needed when you need to use RDTSC
 * (or get_cycles or vread that possibly accesses the TSC) in a defined
 * code region.
 */
static __always_inline void rdtsc_barrier(void)
{
	alternative_2("", "mfence", X86_FEATURE_MFENCE_RDTSC,
			  "lfence", X86_FEATURE_LFENCE_RDTSC);
}

#endif /* _ASM_X86_BARRIER_H */
