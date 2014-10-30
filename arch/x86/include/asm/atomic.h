#ifndef _ASM_X86_ATOMIC_H
#define _ASM_X86_ATOMIC_H

#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/ktsan.h>
#include <asm/processor.h>
#include <asm/alternative.h>
#include <asm/cmpxchg.h>
#include <asm/rmwcc.h>
#include <asm/barrier.h>

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 */

#define ATOMIC_INIT(i)	{ (i) }

/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically reads the value of @v.
 */
static __always_inline int atomic_read(const atomic_t *v)
{
#ifdef CONFIG_KTSAN
	return ktsan_atomic32_read(v);
#else
	return ACCESS_ONCE((v)->counter);
#endif
}

/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 *
 * Atomically sets the value of @v to @i.
 */
static __always_inline void atomic_set(atomic_t *v, int i)
{
#ifdef CONFIG_KTSAN
	ktsan_atomic32_set(v, i);
#else
	v->counter = i;
#endif
}

/**
 * atomic_add - add integer to atomic variable
 * @i: integer value to add
 * @v: pointer of type atomic_t
 *
 * Atomically adds @i to @v.
 */
static __always_inline void atomic_add(int i, atomic_t *v)
{
#ifdef CONFIG_KTSAN
	ktsan_atomic32_add(v, i);
#else
	asm volatile(LOCK_PREFIX "addl %1,%0"
		     : "+m" (v->counter)
		     : "ir" (i));
#endif
}

/**
 * atomic_sub - subtract integer from atomic variable
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 *
 * Atomically subtracts @i from @v.
 */
static __always_inline void atomic_sub(int i, atomic_t *v)
{
#ifdef CONFIG_KTSAN
	ktsan_atomic32_sub(v, i);
#else
	asm volatile(LOCK_PREFIX "subl %1,%0"
		     : "+m" (v->counter)
		     : "ir" (i));
#endif
}

/**
 * atomic_sub_and_test - subtract value from variable and test result
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 *
 * Atomically subtracts @i from @v and returns
 * true if the result is zero, or false for all
 * other cases.
 */
static __always_inline int atomic_sub_and_test(int i, atomic_t *v)
{
#ifdef CONFIG_KTSAN
	return ktsan_atomic32_sub_and_test(v, i);
#else
	GEN_BINARY_RMWcc(LOCK_PREFIX "subl", v->counter, "er", i, "%0", "e");
#endif
}

/**
 * atomic_inc - increment atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically increments @v by 1.
 */
static __always_inline void atomic_inc(atomic_t *v)
{
#ifdef CONFIG_KTSAN
	ktsan_atomic32_inc(v);
#else
	asm volatile(LOCK_PREFIX "incl %0"
		     : "+m" (v->counter));
#endif
}

/**
 * atomic_dec - decrement atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically decrements @v by 1.
 */
static __always_inline void atomic_dec(atomic_t *v)
{
#ifdef CONFIG_KTSAN
	ktsan_atomic32_dec(v);
#else
	asm volatile(LOCK_PREFIX "decl %0"
		     : "+m" (v->counter));
#endif
}

/**
 * atomic_dec_and_test - decrement and test
 * @v: pointer of type atomic_t
 *
 * Atomically decrements @v by 1 and
 * returns true if the result is 0, or false for all other
 * cases.
 */
static __always_inline int atomic_dec_and_test(atomic_t *v)
{
#ifdef CONFIG_KTSAN
	return ktsan_atomic32_dec_and_test(v);
#else
	GEN_UNARY_RMWcc(LOCK_PREFIX "decl", v->counter, "%0", "e");
#endif
}

/**
 * atomic_inc_and_test - increment and test
 * @v: pointer of type atomic_t
 *
 * Atomically increments @v by 1
 * and returns true if the result is zero, or false for all
 * other cases.
 */
static __always_inline int atomic_inc_and_test(atomic_t *v)
{
#ifdef CONFIG_KTSAN
	return ktsan_atomic32_inc_and_test(v);
#else
	GEN_UNARY_RMWcc(LOCK_PREFIX "incl", v->counter, "%0", "e");
#endif
}

/**
 * atomic_add_negative - add and test if negative
 * @i: integer value to add
 * @v: pointer of type atomic_t
 *
 * Atomically adds @i to @v and returns true
 * if the result is negative, or false when
 * result is greater than or equal to zero.
 */
static __always_inline int atomic_add_negative(int i, atomic_t *v)
{
#ifdef CONFIG_KTSAN
	return ktsan_atomic32_add_negative(v, i);
#else
	GEN_BINARY_RMWcc(LOCK_PREFIX "addl", v->counter, "er", i, "%0", "s");
#endif
}

/**
 * atomic_add_return - add integer and return
 * @i: integer value to add
 * @v: pointer of type atomic_t
 *
 * Atomically adds @i to @v and returns @i + @v
 */
static __always_inline int atomic_add_return(int i, atomic_t *v)
{
	/* ktsan: xadd intercepted in cmpxchg.h */
	return i + xadd(&v->counter, i);
}

/**
 * atomic_sub_return - subtract integer and return
 * @v: pointer of type atomic_t
 * @i: integer value to subtract
 *
 * Atomically subtracts @i from @v and returns @v - @i
 */
static __always_inline int atomic_sub_return(int i, atomic_t *v)
{
	return atomic_add_return(-i, v);
}

#define atomic_inc_return(v)  (atomic_add_return(1, v))
#define atomic_dec_return(v)  (atomic_sub_return(1, v))

static __always_inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
	/* ktsan: cmpxchg intercepted in cmpxchg.h */
	return cmpxchg(&v->counter, old, new);
}

static inline int atomic_xchg(atomic_t *v, int new)
{
	/* ktsan: xchg intercepted in cmpxchg.h */
	return xchg(&v->counter, new);
}

/**
 * __atomic_add_unless - add unless the number is already a given value
 * @v: pointer of type atomic_t
 * @a: the amount to add to v...
 * @u: ...unless v is equal to u.
 *
 * Atomically adds @a to @v, so long as @v was not already @u.
 * Returns the old value of @v.
 */
static __always_inline int __atomic_add_unless(atomic_t *v, int a, int u)
{
	int c, old;
	c = atomic_read(v);
	for (;;) {
		if (unlikely(c == (u)))
			break;
		old = atomic_cmpxchg((v), c, c + (a));
		if (likely(old == c))
			break;
		c = old;
	}
	return c;
}

/**
 * atomic_inc_short - increment of a short integer
 * @v: pointer to type int
 *
 * Atomically adds 1 to @v
 * Returns the new value of @u
 */
static __always_inline short int atomic_inc_short(short int *v)
{
	/* FIXME(xairy): ktsan? */
	asm(LOCK_PREFIX "addw $1, %0" : "+m" (*v));
	return *v;
}

/* These are x86-specific, used by some header files */
/* FIXME(xairy): ktsan? */
#define atomic_clear_mask(mask, addr)				\
	asm volatile(LOCK_PREFIX "andl %0,%1"			\
		     : : "r" (~(mask)), "m" (*(addr)) : "memory")

/* FIXME(xairy): ktsan? */
#define atomic_set_mask(mask, addr)				\
	asm volatile(LOCK_PREFIX "orl %0,%1"			\
		     : : "r" ((unsigned)(mask)), "m" (*(addr))	\
		     : "memory")

#ifdef CONFIG_X86_32
# include <asm/atomic64_32.h>
#else
# include <asm/atomic64_64.h>
#endif

#endif /* _ASM_X86_ATOMIC_H */
