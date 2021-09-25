#include <syscall.h>
#include <stdint.h>
#include <task.h>

#define MAX_SYSTEM_CALL_NR 128
typedef uint64_t (* system_call_t)(const struct pt_regs *regs);
system_call_t system_call_table[MAX_SYSTEM_CALL_NR];

/* 系统调用函数声明 */
#define SYSFUNC_DEF(name)	_SYSFUNC_DEF_(name,__NR_##name)
#define _SYSFUNC_DEF_(name,nr)	__SYSFUNC_DEF__(name,nr)
#define __SYSFUNC_DEF__(name,nr)	\
__asm__	(		\
".global "#name"	\n\t"	\
".type	"#name",	@function \n\t"	\
#name":		\n\t"	\
"pushq   %rbp	\n\t"	\
"movq    %rsp,	%rbp	\n\t"	\
"movq	$"#nr",	%rax	\n\t"	\
"jmp	SYSCALL_ENTER	\n\t"	\
);

__asm__	(
"SYSCALL_ENTER:	\n\t"		
"pushq	%r10	\n\t"	
"pushq	%r11	\n\t"	
"leaq	sysexit_return_address(%rip),	%r10	\n\t"	
"movq	%rsp,	%r11		\n\t"	
"sysenter			\n\t"	
"sysexit_return_address:	\n\t"	
"xchgq	%rdx,	%r10	\n\t"	
"xchgq	%rcx,	%r11	\n\t"	
"popq	%r11	\n\t"	
"popq	%r10	\n\t"	
"cmpq	$-0x1000,	%rax	\n\t"	
"jb	SYSCALL_RET	\n\t"	
"movq	%rax,	errno(%rip)	\n\t"	
"orq	$-1,	%rax	\n\t"	
"SYSCALL_RET:	\n\t"	
"leaveq	\n\t"
"retq	\n\t"	
);

SYSFUNC_DEF(getpid)
SYSFUNC_DEF(fork)

uint64_t system_call_function(const struct pt_regs *regs)
{
    return system_call_table[regs->rax](regs);
}

pid_t sys_getpid(const struct pt_regs *regs)
{
    return current->pid;
}

pid_t sys_fork(const struct pt_regs *regs)
{
    struct pt_regs *regs_tmp = (struct pt_regs *)current->thread->rsp0 - 1;
    return do_fork(regs_tmp);
}

void init_syscall(void)
{
    system_call_table[__NR_getpid] = sys_getpid;
    system_call_table[__NR_fork] = sys_fork;
}