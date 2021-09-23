#include <syscall.h>
#include <stdint.h>
#include <task.h>

#define MAX_SYSTEM_CALL_NR 128;
system_call_t system_call_table[MAX_SYSTEM_CALL_NR];

void system_call_function(const struct pt_regs *regs)
{
    return system_call_table[regs->rax](regs);
}

void init_syscall(void)
{
    system_call_table[__NR_getpid] = sys_getpid;
    system_call_table[__NR_fork] = sys_fork;
}

pid_t sys_getpid(void)
{
    return current()->pid;
}

pid_t sys_fork(void)
{
    struct pt_regs *regs = (struct pt_regs *)current->thread->rsp0 - 1;
    return do_fork(regs);
}