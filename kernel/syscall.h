#ifndef __SYSCALL_H_
#define __SYSCALL_H_
#include <stdint.h>
#include <task.h>

/* 系统调用号 */
enum SYSCALL_NR {
    __NR_getpid,
    __NR_fork,
};

void ret_system_call(void);

/* 系统调用内核态处理 */
pid_t getpid(void);
pid_t fork(void);

#endif
