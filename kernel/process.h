/*
 *  kernel/process.h
 *
 *  (C) 2021  Jacky
 */

#ifndef PROCESS_H
#define PROCESS_H

#include "stdint.h"
#include "kernel/thread.h"

/* 用户进程虚拟地址内存池起始地址 */
#define USER_VADDR_START 0x8048000
/* 用户进程最大的栈地址 */
#define USER_VADDR_STACK 0xc0000000

/* 激活线程或进程页表 */
void Process_Activate(Task *task);
/* 进程创建 */
void Process_Create(void *fileName, char *name);
/* 创建进程页目录表 */
uint32_t *Process_PageDir(void);

#endif