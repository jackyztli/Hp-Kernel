#ifndef __SYSCALL_H_
#define __SYSCALL_H_
#include <stdint.h>
#include <task.h>

typedef system_call_t uintptr_t;

/* 系统调用号 */
enum SYSCALL_NR {
    __NR_getpid,
    __NR_fork,
};

void ret_system_call(void);

/* 系统调用内核态处理 */
pid_t sys_getpid(void);
pid_t sys_fork(void);

/* 系统调用函数声明 */
#define SYSFUNC_DEF(name)   __SYSFUNC_DEF__(name, __NR_##name)
#define __SYSFUNC_DEF__(name, nr)   \
__asm__ volatile (          \
    ".global "#name"    "   \
    ""#name":    "          \
    "pushq   %rbp"          \
    "movq    %rsp,	%rbp"   \
    "movq	$"#nr",	%rax"   \
    "jmp SYSCALL_ENTER"     \
);

SYSFUNC_DEF(getpid)
SYSFUNC_DEF(fork)
#endif