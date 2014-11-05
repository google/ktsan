#ifndef ASM_X86_CMPXCHG_H
#define ASM_X86_CMPXCHG_H

#include <linux/bug.h>
#include <linux/compiler.h>
#include <linux/ktsan.h>
#include <asm/alternative.h> /* Provides LOCK_PREFIX */

#define __HAVE_ARCH_CMPXCHG 1

/*
 * Non-existant functions to indicate usage errors at link time
 * (or compile-time if the compiler implements __compiletime_error().
 */
extern void __xchg_wrong_size(void)
	__compiletime_error("Bad argument size for xchg");
extern void __cmpxchg_wrong_size(void)
	__compiletime_error("Bad argument size for cmpxchg");
extern void __xadd_wrong_size(void)
	__compiletime_error("Bad argument size for xadd");
extern void __add_wrong_size(void)
	__compiletime_error("Bad argument size for add");

/*
 * Constants for operation sizes. On 32-bit, the 64-bit size it set to
 * -1 because sizeof will never return -1, thereby making those switch
 * case statements guaranteeed dead code which the compiler will
 * eliminate, and allowing the "missing symbol in the default case" to
 * indicate a usage error.
 */
#define __X86_CASE_B	1
#define __X86_CASE_W	2
#define __X86_CASE_L	4
#ifdef CONFIG_64BIT
#define __X86_CASE_Q	8
#else
#define	__X86_CASE_Q	-1		/* sizeof will never return -1 */
#endif

/* 
 * An exchange-type operation, which takes a value and a pointer, and
 * returns the old value.
 */
#define __xchg_op(ptr, arg, op, lock)					\
	({								\
	        __typeof__ (*(ptr)) __ret = (arg);			\
		switch (sizeof(*(ptr))) {				\
		case __X86_CASE_B:					\
			asm volatile (lock #op "b %b0, %1\n"		\
				      : "+q" (__ret), "+m" (*(ptr))	\
				      : : "memory", "cc");		\
			break;						\
		case __X86_CASE_W:					\
			asm volatile (lock #op "w %w0, %1\n"		\
				      : "+r" (__ret), "+m" (*(ptr))	\
				      : : "memory", "cc");		\
			break;						\
		case __X86_CASE_L:					\
			asm volatile (lock #op "l %0, %1\n"		\
				      : "+r" (__ret), "+m" (*(ptr))	\
				      : : "memory", "cc");		\
			break;						\
		case __X86_CASE_Q:					\
			asm volatile (lock #op "q %q0, %1\n"		\
				      : "+r" (__ret), "+m" (*(ptr))	\
				      : : "memory", "cc");		\
			break;						\
		default:						\
			__ ## op ## _wrong_size();			\
		}							\
		__ret;							\
	})

#ifndef CONFIG_KTSAN
/*
 * Note: no "lock" prefix even on SMP: xchg always implies lock anyway.
 * Since this is generally used to protect other memory information, we
 * use "asm volatile" and "memory" clobbers to prevent gcc from moving
 * information around.
 */
#define xchg(ptr, v)	__xchg_op((ptr), (v), xchg, "")
#else /* CONFIG_KTSAN */
#define xchg(ptr, v)							\
({									\
	__typeof__(*(ptr)) ret;						\
	s64 ret64;							\
	s32 ret32;							\
									\
	__typeof__(*(ptr)) lv = (v);					\
	s64 s64lv = *((s64 *)(&lv));					\
	s32 s32lv = *((s32 *)(&lv));					\
									\
	void* vptr = (void *)(ptr);					\
									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8 &&				\
		     sizeof(*(ptr)) != 4);				\
									\
	if (sizeof(*(ptr)) == 8) {					\
		ret64 = ktsan_atomic64_xchg(vptr, s64lv);		\
		ret = *((__typeof__(ptr))(&ret64));			\
	} else if (sizeof(*(ptr)) == 4) {				\
		ret32 = ktsan_atomic32_xchg(vptr, s32lv);		\
		ret = *((__typeof__(ptr))(&ret32));			\
	}								\
									\
	ret;								\
})
#endif /* CONFIG_KTSAN */

/*
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */
#define __raw_cmpxchg(ptr, old, new, size, lock)			\
({									\
	__typeof__(*(ptr)) __ret;					\
	__typeof__(*(ptr)) __old = (old);				\
	__typeof__(*(ptr)) __new = (new);				\
	switch (size) {							\
	case __X86_CASE_B:						\
	{								\
		volatile u8 *__ptr = (volatile u8 *)(ptr);		\
		asm volatile(lock "cmpxchgb %2,%1"			\
			     : "=a" (__ret), "+m" (*__ptr)		\
			     : "q" (__new), "0" (__old)			\
			     : "memory");				\
		break;							\
	}								\
	case __X86_CASE_W:						\
	{								\
		volatile u16 *__ptr = (volatile u16 *)(ptr);		\
		asm volatile(lock "cmpxchgw %2,%1"			\
			     : "=a" (__ret), "+m" (*__ptr)		\
			     : "r" (__new), "0" (__old)			\
			     : "memory");				\
		break;							\
	}								\
	case __X86_CASE_L:						\
	{								\
		volatile u32 *__ptr = (volatile u32 *)(ptr);		\
		asm volatile(lock "cmpxchgl %2,%1"			\
			     : "=a" (__ret), "+m" (*__ptr)		\
			     : "r" (__new), "0" (__old)			\
			     : "memory");				\
		break;							\
	}								\
	case __X86_CASE_Q:						\
	{								\
		volatile u64 *__ptr = (volatile u64 *)(ptr);		\
		asm volatile(lock "cmpxchgq %2,%1"			\
			     : "=a" (__ret), "+m" (*__ptr)		\
			     : "r" (__new), "0" (__old)			\
			     : "memory");				\
		break;							\
	}								\
	default:							\
		__cmpxchg_wrong_size();					\
	}								\
	__ret;								\
})

#define __cmpxchg(ptr, old, new, size)					\
	__raw_cmpxchg((ptr), (old), (new), (size), LOCK_PREFIX)

#define __sync_cmpxchg(ptr, old, new, size)				\
	__raw_cmpxchg((ptr), (old), (new), (size), "lock; ")

#define __cmpxchg_local(ptr, old, new, size)				\
	__raw_cmpxchg((ptr), (old), (new), (size), "")

#ifdef CONFIG_X86_32
# include <asm/cmpxchg_32.h>
#else
# include <asm/cmpxchg_64.h>
#endif

#ifndef CONFIG_KTSAN
#define cmpxchg(ptr, old, new)						\
	__cmpxchg(ptr, old, new, sizeof(*(ptr)))
#else /* CONFIG_KTSAN */
#define cmpxchg(ptr, old, new)						\
({									\
	__typeof__(*(ptr)) ret;						\
	s64 ret64;							\
	s32 ret32;							\
	s16 ret16;							\
									\
	__typeof__(*(ptr)) lo = (old);					\
	s64 s64lo = *((s64 *)(&lo));					\
	s32 s32lo = *((s32 *)(&lo));					\
	s16 s16lo = *((s16 *)(&lo));					\
									\
	__typeof__(*(ptr)) ln = (new);					\
	s64 s64ln = *((s64 *)(&ln));					\
	s32 s32ln = *((s32 *)(&ln));					\
	s16 s16ln = *((s16 *)(&ln));					\
									\
	void* vptr = (void *)(ptr);					\
									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8 &&				\
		     sizeof(*(ptr)) != 4 &&				\
		     sizeof(*(ptr)) != 2);				\
									\
	if (sizeof(*(ptr)) == 8) {					\
		ret64 = ktsan_atomic64_cmpxchg(vptr, s64lo, s64ln);	\
		ret = *((__typeof__(ptr))(&ret64));			\
	} else if (sizeof(*(ptr)) == 4) {				\
		ret32 = ktsan_atomic32_cmpxchg(vptr, s32lo, s32ln);	\
		ret = *((__typeof__(ptr))(&ret32));			\
	} else if (sizeof(*(ptr)) == 2) {				\
		ret16 = ktsan_atomic16_cmpxchg(vptr, s16lo, s16ln);	\
		ret = *((__typeof__(ptr))(&ret16));			\
	}								\
									\
	ret;								\
})
#endif /* CONFIG_KTSAN */

/* FIXME(xairy): ktsan? */
#define sync_cmpxchg(ptr, old, new)					\
	__sync_cmpxchg(ptr, old, new, sizeof(*(ptr)))

/* FIXME(xairy): ktsan? */
#define cmpxchg_local(ptr, old, new)					\
	__cmpxchg_local(ptr, old, new, sizeof(*(ptr)))

/*
 * xadd() adds "inc" to "*ptr" and atomically returns the previous
 * value of "*ptr".
 *
 * xadd() is locked when multiple CPUs are online
 * xadd_sync() is always locked
 * xadd_local() is never locked
 */

#define __xadd(ptr, inc, lock)	__xchg_op((ptr), (inc), xadd, lock)

#ifndef CONFIG_KTSAN
#define xadd(ptr, inc)		__xadd((ptr), (inc), LOCK_PREFIX)
#else /* CONFIG_KTSAN */
#define xadd(ptr, inc)							\
({									\
	__typeof__(*(ptr)) ret;						\
	s64 ret64;							\
	s32 ret32;							\
	s16 ret16;							\
									\
	__typeof__(*(ptr)) li = (inc);					\
	s64 s64li = *((s64 *)(&li));					\
	s32 s32li = *((s32 *)(&li));					\
	s16 s16li = *((s16 *)(&li));					\
									\
	void* vptr = (void *)(ptr);					\
									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8 &&				\
		     sizeof(*(ptr)) != 4 &&				\
		     sizeof(*(ptr)) != 2);				\
									\
	if (sizeof(*(ptr)) == 8) {					\
		ret64 = ktsan_atomic64_xadd(vptr, s64li);		\
		ret = *((__typeof__(ptr))(&ret64));			\
	} else if (sizeof(*(ptr)) == 4) {				\
		ret32 = ktsan_atomic32_xadd(vptr, s32li);		\
		ret = *((__typeof__(ptr))(&ret32));			\
	} else if (sizeof(*(ptr)) == 2) {				\
		ret16 = ktsan_atomic16_xadd(vptr, s16li);		\
		ret = *((__typeof__(ptr))(&ret16));			\
	}								\
									\
	ret;								\
})
#endif /* CONFIG_KTSAN */

/* FIXME(xairy): ktsan? */
#define xadd_sync(ptr, inc)	__xadd((ptr), (inc), "lock; ")

/* FIXME(xairy): ktsan? */
#define xadd_local(ptr, inc)	__xadd((ptr), (inc), "")

/* FIXME(xairy): ktsan here and below? */
#define __add(ptr, inc, lock)						\
	({								\
	        __typeof__ (*(ptr)) __ret = (inc);			\
		switch (sizeof(*(ptr))) {				\
		case __X86_CASE_B:					\
			asm volatile (lock "addb %b1, %0\n"		\
				      : "+m" (*(ptr)) : "qi" (inc)	\
				      : "memory", "cc");		\
			break;						\
		case __X86_CASE_W:					\
			asm volatile (lock "addw %w1, %0\n"		\
				      : "+m" (*(ptr)) : "ri" (inc)	\
				      : "memory", "cc");		\
			break;						\
		case __X86_CASE_L:					\
			asm volatile (lock "addl %1, %0\n"		\
				      : "+m" (*(ptr)) : "ri" (inc)	\
				      : "memory", "cc");		\
			break;						\
		case __X86_CASE_Q:					\
			asm volatile (lock "addq %1, %0\n"		\
				      : "+m" (*(ptr)) : "ri" (inc)	\
				      : "memory", "cc");		\
			break;						\
		default:						\
			__add_wrong_size();				\
		}							\
		__ret;							\
	})

/*
 * add_*() adds "inc" to "*ptr"
 *
 * __add() takes a lock prefix
 * add_smp() is locked when multiple CPUs are online
 * add_sync() is always locked
 */
#define add_smp(ptr, inc)	__add((ptr), (inc), LOCK_PREFIX)
#define add_sync(ptr, inc)	__add((ptr), (inc), "lock; ")

#define __cmpxchg_double(pfx, p1, p2, o1, o2, n1, n2)			\
({									\
	bool __ret;							\
	__typeof__(*(p1)) __old1 = (o1), __new1 = (n1);			\
	__typeof__(*(p2)) __old2 = (o2), __new2 = (n2);			\
	BUILD_BUG_ON(sizeof(*(p1)) != sizeof(long));			\
	BUILD_BUG_ON(sizeof(*(p2)) != sizeof(long));			\
	VM_BUG_ON((unsigned long)(p1) % (2 * sizeof(long)));		\
	VM_BUG_ON((unsigned long)((p1) + 1) != (unsigned long)(p2));	\
	asm volatile(pfx "cmpxchg%c4b %2; sete %0"			\
		     : "=a" (__ret), "+d" (__old2),			\
		       "+m" (*(p1)), "+m" (*(p2))			\
		     : "i" (2 * sizeof(long)), "a" (__old1),		\
		       "b" (__new1), "c" (__new2));			\
	__ret;								\
})

#define cmpxchg_double(p1, p2, o1, o2, n1, n2) \
	__cmpxchg_double(LOCK_PREFIX, p1, p2, o1, o2, n1, n2)

#define cmpxchg_double_local(p1, p2, o1, o2, n1, n2) \
	__cmpxchg_double(, p1, p2, o1, o2, n1, n2)

#endif	/* ASM_X86_CMPXCHG_H */
