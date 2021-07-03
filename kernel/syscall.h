/*
 *  kernel/syscall.h
 *
 *  (C) 2021  Jacky
 */
#ifndef SYSCALL_H
#define SYSCALL_H

#include "stdint.h"
#include "kernel/thread.h"

/* 无参系统调用 */
#define _syscall0(NUMBER) ({                                               \
    int32_t ret;                                                           \
    __asm__ volatile ("int $0x80" : "=a" (ret) : "a" (NUMBER) : "memory"); \
    ret;                                                                   \
})

/* 一个参系统调用 */
#define _syscall1(NUMBER, ARG1) ({                                                    \
    int32_t ret;                                                                      \
    __asm__ volatile ("int $0x80" : "=a" (ret) : "a" (NUMBER), "b" (ARG1): "memory"); \
    ret;                                                                              \
})

/* 三个参系统调用 */
#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({                                        \
    int32_t ret;                                                                      \
    __asm__ volatile ("int $0x80"                                                     \
    : "=a" (ret)                                                                      \
    : "a" (NUMBER), "b" (ARG1), "c" (ARG2), "d" (ARG3): "memory");                    \
    ret;                                                                              \
})

typedef enum {
    SYS_GETPID,
    SYS_READ,
    SYS_WRITE,
    SYS_MALLOC,
    SYS_FREE,
    SYS_FORK,

    SYS_BUTT
} SYSCALL_NR;

/* 用户态系统调用API */
pid_t getpid(void);
int32_t read(int32_t fd, void *buf, uint32_t count);
int32_t write(int32_t fd, const void *buf, uint32_t count);
void *malloc(uint32_t size);
void free(void *ptr);
pid_t fork(void);

pid_t sys_getpid(void);

/* 系统调用模块初始化 */
void Syscall_Init(void);

#endif