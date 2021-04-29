/*
 *  kernel/memory.h
 *
 *  (C) 2021  Jacky
 */
#ifndef MEMORY_H
#define MEMORY_H

#include "stdint.h"
#include "kernel/bitmap.h"
#include "lib/list.h"

/* 页表大小为4K */
#define PAGE_SIZE 4096

/* 页表属性宏 */
#define PG_P_0 0      /* 页目录项或页表被未占用标志 */
#define PG_P_1 1      /* 页目录项或页表被占用标志 */
#define PG_RW_R 0     /* R/W属性位，读/执行 */
#define PG_RW_W 2     /* R/W属性位，读/写/执行 */
#define PG_US_S 0     /* 设置访问权限，系统级 */
#define PG_US_U 4     /* 设置访问权限，用户级 */

/* 虚拟地址内存池 */
typedef struct {
    Bitmap bitmap;
    uint32_t virtualAddrStart;
} VirtualMemPool;

/* 虚拟地址分配类型 */
typedef enum {
    VIR_MEM_KERNEL,    /* 分配内核虚拟地址 */
    VIR_MEM_USER       /* 分配用户虚拟地址 */
} VirMemType;

/* 内存块 */
typedef struct {
    ListNode freeNode;
} MemBlock;

/* 内存块描述符 */
typedef struct {
    uint32_t blockSize;
    uint32_t blockPerArena;
    List freeList;
} MemBlockDesc;

/* 内存仓库 */
typedef struct {
    MemBlockDesc *desc;
    uint32_t cnt;
    bool large;
} MemArena;

/* 内存块描述符个数 */
#define DESC_CNT 7

/* 内存管理模块初始化入口 */
void Mem_Init(void);
/* 申请一页空间，并映射到指定地址 */
void *Mem_GetOnePage(VirMemType virMemType, uintptr_t virAddrStart);
/* 内核申请n个页空间 */
void *Mem_GetKernelPages(uint32_t pageNum);
/* 根据虚拟地址获取对应物理地址 */
uintptr_t Mem_V2P(uintptr_t virAddr);

/* 初始化内核内存块描述符数组 */
void Mem_BlockDescInit(MemBlockDesc *memBlockDesc);
/* 在堆上申请size大小字节内存 */
void *Mem_Malloc(uint32_t size);

void *sys_malloc(uint32_t size);
void sys_free(void *addr);

#endif