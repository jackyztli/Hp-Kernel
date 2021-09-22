#ifndef __SYSCALL_H_
#define __SYSCALL_H_
#include <stdint.h>
#include <task.h>

typedef system_call_t uintptr_t;

/* 系统调用号 */
enum SYSCALL_NR {
    SYS_GETPID,
};

void ret_system_call(void);

/* 系统调用内核态处理 */
pid_t sys_getpid(void);

#endif