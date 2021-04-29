/*
 *  kernel/syscall.c
 *
 *  (C) 2021  Jacky
 */

#include "syscall.h"
#include "stdint.h"
#include "kernel/thread.h"
#include "kernel/console.h"
#include "kernel/panic.h"
#include "lib/string.h"

typedef void *syscall_handler;
syscall_handler syscall_table[SYS_BUTT];

pid_t sys_getpid(void)
{
    return Thread_GetRunningTask()->pid;
}

pid_t getpid(void)
{
    return _syscall0(SYS_GETPID);
}

uint32_t sys_write(const char *str)
{
    ASSERT(str != NULL);
    Console_PutStr(str);
    return  strlen(str);
}

uint32_t write(const char *str)
{
    return _syscall1(SYS_WRITE, str);
}

void *malloc(uint32_t size)
{
    return _syscall1(SYS_MALLOC, size);
}

void free(void *ptr)
{
    return _syscall1(SYS_FREE, ptr);
}

/* 系统调用模块初始化 */
void Syscall_Init(void)
{
    Console_PutStr("Syscall_Init start.\n");    
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;
    Console_PutStr("Syscall_Init end.\n"); 

    return;
}