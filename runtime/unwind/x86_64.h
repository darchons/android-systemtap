/* -*- linux-c -*-
 *
 * x86_64 dwarf unwinder header file
 * Copyright (C) 2008, 2010 Red Hat Inc.
 * Copyright (C) 2002-2006 Novell, Inc.
 * 
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */
#ifndef _STP_X86_64_UNWIND_H
#define _STP_X86_64_UNWIND_H

/*
 * Copyright (C) 2002-2006 Novell, Inc.
 *	Jan Beulich <jbeulich@novell.com>
 * This code is released under version 2 of the GNU GPL.
 */

#include <linux/sched.h>
#include <asm/ptrace.h>

/* these are simple for x86_64 */
#define _stp_get_unaligned(ptr) (*(ptr))
#define _stp_put_unaligned(val, ptr) ((void)( *(ptr) = (val) ))

struct unwind_frame_info
{
	struct pt_regs regs;
	struct task_struct *task;
	unsigned call_frame:1;
};

#ifdef STAPCONF_X86_UNIREGS
#define UNW_PC(frame)        (frame)->regs.ip
#define UNW_SP(frame)        (frame)->regs.sp
#else
#define UNW_PC(frame)        (frame)->regs.rip
#define UNW_SP(frame)        (frame)->regs.rsp
#endif /* STAPCONF_X86_UNIREGS */

#if 0 /* STP_USE_FRAME_POINTER */
/* Frame pointers not implemented in x86_64 currently */
#define UNW_FP(frame)        (frame)->regs.rbp
#define FRAME_RETADDR_OFFSET 8
#define FRAME_LINK_OFFSET    0
#define STACK_BOTTOM(tsk)    (((tsk)->thread.rsp0 - 1) & ~(THREAD_SIZE - 1))
#define STACK_TOP(tsk)       ((tsk)->thread.rsp0)
#endif

/* Might need to account for the special exception and interrupt handling
   stacks here, since normally
	EXCEPTION_STACK_ORDER < THREAD_ORDER < IRQSTACK_ORDER,
   but the construct is needed only for getting across the stack switch to
   the interrupt stack - thus considering the IRQ stack itself is unnecessary,
   and the overhead of comparing against all exception handling stacks seems
   not desirable. */
#define STACK_LIMIT(ptr)     (((ptr) - 1) & ~(THREAD_SIZE - 1))

#ifdef STAPCONF_X86_UNIREGS
#define UNW_REGISTER_INFO \
	PTREGS_INFO(ax), \
	PTREGS_INFO(dx), \
	PTREGS_INFO(cx), \
	PTREGS_INFO(bx), \
	PTREGS_INFO(si), \
	PTREGS_INFO(di), \
	PTREGS_INFO(bp), \
	PTREGS_INFO(sp), \
	PTREGS_INFO(r8), \
	PTREGS_INFO(r9), \
	PTREGS_INFO(r10), \
	PTREGS_INFO(r11), \
	PTREGS_INFO(r12), \
	PTREGS_INFO(r13), \
	PTREGS_INFO(r14), \
	PTREGS_INFO(r15), \
	PTREGS_INFO(ip)
#else
#define UNW_REGISTER_INFO \
	PTREGS_INFO(rax), \
	PTREGS_INFO(rdx), \
	PTREGS_INFO(rcx), \
	PTREGS_INFO(rbx), \
	PTREGS_INFO(rsi), \
	PTREGS_INFO(rdi), \
	PTREGS_INFO(rbp), \
	PTREGS_INFO(rsp), \
	PTREGS_INFO(r8), \
	PTREGS_INFO(r9), \
	PTREGS_INFO(r10), \
	PTREGS_INFO(r11), \
	PTREGS_INFO(r12), \
	PTREGS_INFO(r13), \
	PTREGS_INFO(r14), \
	PTREGS_INFO(r15), \
	PTREGS_INFO(rip)
#endif /* STAPCONF_X86_UNIREGS */

#define UNW_DEFAULT_RA(raItem, dataAlign) \
	((raItem).where == Memory && \
	 !((raItem).value * (dataAlign) + 8))

static inline void arch_unw_init_frame_info(struct unwind_frame_info *info,
                                            /*const*/ struct pt_regs *regs,
					    int sanitize)
{
	memset(info, 0, sizeof(*info));
	if (sanitize) {
		info->regs.r11 = regs->r11;
		info->regs.r10 = regs->r10;
		info->regs.r9 = regs->r9;
		info->regs.r8 = regs->r8;
#ifdef STAPCONF_X86_UNIREGS
		info->regs.ax = regs->ax;
		info->regs.cx = regs->cx;
		info->regs.dx = regs->dx;
		info->regs.si = regs->si;
		info->regs.di = regs->di;
		info->regs.orig_ax = regs->orig_ax;
		info->regs.ip = regs->ip;
		info->regs.flags = regs->flags;
		info->regs.sp = regs->sp;
#else
		info->regs.rax = regs->rax;
		info->regs.rcx = regs->rcx;
		info->regs.rdx = regs->rdx;
		info->regs.rsi = regs->rsi;
		info->regs.rdi = regs->rdi;
		info->regs.orig_rax = regs->orig_rax;
		info->regs.rip = regs->rip;
		info->regs.eflags = regs->eflags;
		info->regs.rsp = regs->rsp;
#endif
		info->regs.cs = regs->cs;
		info->regs.ss = regs->ss;
	} else {
		info->regs = *regs;
	}
}

static inline void arch_unw_init_blocked(struct unwind_frame_info *info)
{
	extern const char thread_return[];

	memset(&info->regs, 0, sizeof(info->regs));
	info->regs.cs = __KERNEL_CS;
	info->regs.ss = __KERNEL_DS;	
	
#ifdef STAPCONF_X86_UNIREGS	
	info->regs.ip = (unsigned long)thread_return;
	__get_user(info->regs.bp, (unsigned long *)info->task->thread.sp);
	info->regs.sp = info->task->thread.sp;
#else
	info->regs.rip = (unsigned long)thread_return;
	__get_user(info->regs.rbp, (unsigned long *)info->task->thread.rsp);
	info->regs.rsp = info->task->thread.rsp;
#endif
}

#endif /* _STP_X86_64_UNWIND_H */
