#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdint.h>

#define PAGE_2M_SHIFT 21
#define PAGE_2M_SIZE (1 << PAGE_2M_SHIFT)
#define PAGE_2M_MASK (~(PAGE_2M_SIZE - 1))
/* 向上2M地址对齐 */
#define PAGE_2M_ALIGN_UP(addr) (((uintptr_t)(addr) + PAGE_2M_SIZE - 1) & PAGE_2M_MASK)
/* 向下2M地址对齐 */
#define PAGE_2M_ALIGN_DOWN(addr) (((uintptr_t)(addr) >> PAGE_2M_SHIFT) << PAGE_2M_SHIFT)
void init_mem(void);

/* 申请一片物理页表，大小为2M */
uintptr_t alloc_page(void);
/* 释放一片物理页表，大小为2M */
void free_page(uintptr_t addr);

#endif