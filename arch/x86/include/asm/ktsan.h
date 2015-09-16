#ifndef _ASM_X86_KTSAN_H
#define _ASM_X86_KTSAN_H

#ifdef CONFIG_KTSAN

#define KTSAN_PUSH_REGS				\
	pushq	%rax;				\
	pushq	%rcx;				\
	pushq	%rdx;				\
	pushq	%rdi;				\
	pushq	%rsi;				\
	pushq	%r8;				\
	pushq	%r9;				\
	pushq	%r10;				\
	pushq	%r11;				\
/**/

#define KTSAN_POP_REGS				\
	popq	%r11;				\
	popq	%r10;				\
	popq	%r9;				\
	popq	%r8;				\
	popq	%rsi;				\
	popq	%rdi;				\
	popq	%rdx;				\
	popq	%rcx;				\
	popq	%rax;				\
/**/

#define KTSAN_INTERRUPT_ENTER			\
	KTSAN_PUSH_REGS				\
	call	ktsan_interrupt_enter;		\
	KTSAN_POP_REGS				\
/**/

#define KTSAN_INTERRUPT_EXIT			\
	call	ktsan_interrupt_exit;		\
/**/

#define KTSAN_SYSCALL_ENTER			\
	KTSAN_PUSH_REGS				\
	call	ktsan_syscall_enter;		\
	KTSAN_POP_REGS				\
/**/

#define KTSAN_SYSCALL_EXIT			\
	call	ktsan_syscall_exit;		\
/**/

#else /* ifdef CONFIG_KTSAN */

#define KTSAN_INTERRUPT_ENTER
#define KTSAN_INTERRUPT_EXIT
#define KTSAN_SYSCALL_ENTER
#define KTSAN_SYSCALL_EXIT

#endif /* ifdef CONFIG_KTSAN */
#endif /* ifndef _ASM_X86_KTSAN_H */
