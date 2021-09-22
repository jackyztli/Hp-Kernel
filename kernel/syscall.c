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
    system_call_table[SYS_GETPID] = sys_getpid;
}

pid_t sys_getpid(void)
{
    return current()->pid;
}