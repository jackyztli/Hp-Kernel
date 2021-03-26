/*
 *  kernel/memory.h
 *
 *  (C) 2021  Jacky
 */
#ifndef MEMORY_H
#define MEMORY_H

#include "stdint.h"
#include "bitmap.h"

/* 页表大小为4K */
#define PAGE_SIZE 4096

/* 物理内存池 */
typedef struct {
    Bitmap bitmap;
    uint32_t phyAddrStart;
    uint32_t poolSize;
} MemPool;

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

/* 内存管理模块初始化入口 */
void Mem_Init(void);
/* 内核申请n个页空间 */
void *Mem_GetKernelPages(uint32_t pageNum);

#endif