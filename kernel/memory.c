/*
 *  kernel/memory.c
 *
 *  (C) 2021  Jacky
 */

#include "memory.h"
#include "stdint.h"
#include "kernel/panic.h"
#include "kernel/bitmap.h"
#include "lib/print.h"
#include "lib/string.h"

/* 内存位图存放地址，选择该位置原因是：
 * 0xc009f000为内核最大栈地址，
 * 预留4K地址（PCB） +  4 * 4K的位图空间（4K位图空间可表示128M内存，这里预估512M）
 * 所以 MEM_BITMAP_BASE = 0xc009f000 - 0x1000 - 0x4000 = 0xc009a000
 */
#define MEM_BITMAP_BASE 0xc009a000

/* loader中计算到的物理内存大小存放路径 */
#define MEM_TOTAL_SIZE_ADDR (0xb03)

/* 虚拟内存池起始地址 */
#define K_HEAP_START 0xc0100000

/* 获取页目录项或页表项宏 */
#define PDE_INDEX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_INDEX(addr) ((addr & 0x003ff000) >> 12)

/* 页表属性宏 */
#define PG_P_0 0      /* 页目录项或页表被未占用标志 */
#define PG_P_1 1      /* 页目录项或页表被占用标志 */
#define PG_RW_R 0     /* R/W属性位，读/执行 */
#define PG_RW_W 1     /* R/W属性位，读/写/执行 */
#define PG_US_S 0     /* 设置访问权限，系统级 */
#define PG_US_U 4     /* 设置访问权限，用户级 */

/* 内核物理内存池 */
MemPool kernelMemPool;
/* 用户物理内存池 */
MemPool userMemPool;

/* 内核虚拟地址内存池 */
VirtualMemPool kernelVirMemPool;

/* 内存池初始化 */
static void Mem_PoolInit(uint32_t allMemSize)
{
    put_str("Mem_PoolInit start. \n");

    /* 页目录(1) + 页表(框号为1) + 页表(框号769 ~ 1022 = 254个) = 256页被占用 */
    uint32_t pageTableSize = 256 * PAGE_SIZE;
    /* 0x100000表示1M低端内存 */
    uint32_t usedMemSize = pageTableSize + 0x100000;
    uint32_t freeMemSize = allMemSize - usedMemSize;
    uint16_t allFreePages = freeMemSize / PAGE_SIZE;

    /* 内核和用户平分所有可用的内存页 */
    uint16_t kernelFreePages = allFreePages / 2;
    uint16_t userFreePages = allFreePages - kernelFreePages;

    /* 初始化内核物理内存池 */
    kernelMemPool.bitmap.bitmapLen = kernelFreePages / 8;
    kernelMemPool.bitmap.bitmap = (uint8_t *)MEM_BITMAP_BASE;
    kernelMemPool.phyAddrStart = usedMemSize;
    kernelMemPool.poolSize = kernelFreePages * PAGE_SIZE;
    /* 将位图置0，表示内存未被使用 */
    BitmapInit(&kernelMemPool.bitmap);

    /* 初始化用户物理内存池 */
    userMemPool.bitmap.bitmapLen = userFreePages / 8;
    userMemPool.bitmap.bitmap = (uint8_t *)(MEM_BITMAP_BASE + kernelMemPool.bitmap.bitmapLen);
    userMemPool.phyAddrStart = kernelMemPool.phyAddrStart + kernelMemPool.poolSize;
    userMemPool.poolSize = userFreePages * PAGE_SIZE;
    /* 将位图置0，表示内存未被使用 */
    BitmapInit(&userMemPool.bitmap);
    
    /* 打印物理内存划分情况 */
    put_str("kernelMemPool.bitmap.bitmap: ");
    put_int((uintptr_t)kernelMemPool.bitmap.bitmap);
    put_str(" kernelMemPool.bitmap.bitmapLen: ");
    put_int(kernelMemPool.bitmap.bitmapLen);
    put_str(" kernelMemPool.phyAddrStart: ");
    put_int(kernelMemPool.phyAddrStart);
    put_str(" kernelMemPool.poolSize: ");
    put_int(kernelMemPool.poolSize);
    put_str("\n");

    put_str("userMemPool.bitmap.bitmap: ");
    put_int((uintptr_t)userMemPool.bitmap.bitmap);
    put_str(" userMemPool.bitmap.bitmapLen: ");
    put_int(userMemPool.bitmap.bitmapLen);
    put_str(" userMemPool.phyAddrStart: ");
    put_int(userMemPool.phyAddrStart);
    put_str(" userMemPool.poolSize: ");
    put_int(userMemPool.poolSize);
    put_str("\n");

    /* 内核虚拟内存池初始化 */
    kernelVirMemPool.bitmap.bitmapLen = kernelMemPool.bitmap.bitmapLen;
    kernelVirMemPool.bitmap.bitmap = (uint8_t *)(MEM_BITMAP_BASE + kernelMemPool.bitmap.bitmapLen + userMemPool.bitmap.bitmapLen);
    kernelVirMemPool.virtualAddrStart = K_HEAP_START;
    /* 将位图置0，表示内存未被使用 */
    BitmapInit(&kernelVirMemPool.bitmap);

    put_str("Mem_PoolInit end, \n");
}

/* 分配n个连续虚拟页，成功返回虚拟地址，失败者返回NULL */
static void *Mem_GetVirAddr(VirMemType virMemtype, uint32_t pagesNums)
{
    uintptr_t virAddr = NULL;

    if (virMemtype == VIR_MEM_KERNEL) {
        /* 通过位图找到合适的空间 */
        int32_t startBitIndex = BitmapScan(&kernelVirMemPool.bitmap, pagesNums);
        /* 找不到n个可用的连续虚拟页 */
        if (startBitIndex == -1) {
            return NULL;
        }

        /* 从startBitIndex开始，将pagesNums位bit置1，表示已被占用 */
        for (uint32_t i = 0; i < pagesNums; i++) {
            BitmapSet(&kernelVirMemPool.bitmap, startBitIndex + i, 1);
        }
        virAddr = kernelVirMemPool.virtualAddrStart + startBitIndex * PAGE_SIZE;
    } else {
        /* 分配用户虚拟地址 */
    }

    return (void *)virAddr;
}

/* 返回虚拟地址virAddr对应的页目录项的虚拟地址指针 */
uint32_t *Mem_GetVirAddrPdePtr(uintptr_t virAddr)
{
    /* 通过pde指针（虚拟地址），系统可以找到virAddr对应页目录项的物理地址 */
    /* 在初始化页目录项时，我们将1023项指向了页目录表物理地址，所以0xfffff000表示获取到页目录表的起始地址（即0x1000000） */
    /* 在页目录项中找到PDE_INDEX(virAddr) * 4的地址，这是取得virAddr对应页目录项的物理地址 */
    uint32_t *pde = (uint32_t *)(0xfffff000 + PDE_INDEX(virAddr) * 4);
    return pde;
}

/* 返回虚拟地址virAddr对应的页表项的虚拟地址指针 */
uint32_t *Mem_GetVirAddrPtePtr(uintptr_t virAddr)
{
    /* 通过pte指针（虚拟地址），系统可以找到virAddr对应页表项的物理地址 */
    /* 在初始化页目录项时，我们将1023项指向了页目录表物理地址，所以0xffc00000表示获取到页目录表的起始地址（即0x1000000） */
    /* 在页目录项中找到(0xffc00000 & virAddr) >> 10项，这是取得virAddr对应页表的物理地址，最后通过页表偏移找到页表项的物理地址 */
    uint32_t *pte = (uint32_t *)(0xffc00000 + ((0xffc00000 & virAddr) >> 10) + PTE_INDEX(virAddr) * 4);
    return pte;
}

/* 从物理内存中分配一个物理页，成功则返回物理地址，失败返回NULL */
static void *Mem_Palloc(MemPool *memPool)
{
    /* 从位图中获取一个可用的bit */
    int32_t bitIndex = BitmapScan(&(memPool->bitmap), 1);
    if (bitIndex == -1) {
        return NULL;
    }

    BitmapSet(&(memPool->bitmap), bitIndex, 1);
    uint32_t pagePhyAddr = memPool->phyAddrStart + bitIndex * PAGE_SIZE;
    return (void *)pagePhyAddr;
}

/* 页表中添加虚拟地址virAddr与物理地址pagePhyAddr的映射 */
static void Mem_AddPageTable(void *virAddr, void *pagePhyAddr)
{
    uint32_t virAddrTmp = (uint32_t)virAddr;
    uint32_t pagePhyAddrTmp = (uint32_t)pagePhyAddr;

    uint32_t *pde = Mem_GetVirAddrPdePtr(virAddrTmp);
    uint32_t *pte = Mem_GetVirAddrPtePtr(virAddrTmp);

    /* 通过P位判断是否页目录项是否存在 */
    if ((*pde) & 0x00000001) {
        /* 如果页表也已经被占用，则系统出错 */
        ASSERT(!((*pte) & 0x00000001));
        /* 设置页表属性 */
        *pte = (pagePhyAddrTmp | PG_US_U | PG_RW_W | PG_P_1);
    } else {
        /* 页目录项不存在，先创建页表映射 */
        uint32_t pdePhyAddr = (uint32_t)Mem_Palloc(&kernelMemPool);
        ASSERT(pagePhyAddr != NULL);

        /* 设置页目录项物理地址和属性 */
        *pde = (pdePhyAddr | PG_US_U | PG_RW_W | PG_P_1);
        /* 清空页表，此处不能memset(pdePhyAddr, 0, PAGE_SIZE), 因为pdePhyAddr是物理地址，需要使用其对应的虚拟地址 */
        memset((void *)((uintptr_t)pte & 0xfffff000), 0, PAGE_SIZE);

        ASSERT(!((*pte) & 0x00000001));

        *pte = (pagePhyAddrTmp | PG_US_U | PG_RW_W | PG_P_1);
    }

    return;
 }

/* 分配n个页空间，成功则返回虚拟地址，失败时返回NULL */
void *Mem_MallocPages(VirMemType virMemType, uint32_t pageNum)
{
    ASSERT(pageNum > 0);

    void *virAddrStart = Mem_GetVirAddr(virMemType, pageNum);
    if (virAddrStart == NULL) {
        return NULL;
    }

    void *virAddrStartTmp = virAddrStart;

    MemPool *memPool = NULL;
    if (virMemType == VIR_MEM_KERNEL) {
        memPool = &kernelMemPool;
    } else if (virMemType == VIR_MEM_USER) {
        memPool = &userMemPool;
    }

    ASSERT(memPool != NULL);

    while (pageNum--) {
        void *pagePhyAddr = Mem_Palloc(memPool);
        if (pagePhyAddr == NULL) {
            return NULL;
        }

        Mem_AddPageTable(virAddrStartTmp, pagePhyAddr);
        virAddrStartTmp += PAGE_SIZE;
    }

    return virAddrStart;
}

/* 内核申请n个页空间 */
void *Mem_GetKernelPages(uint32_t pageNum)
{
    void *virAddrStart = Mem_MallocPages(VIR_MEM_KERNEL, pageNum);
    if (virAddrStart == NULL) {
        return NULL;
    }

    memset(virAddrStart, 0, pageNum * PAGE_SIZE);

    return virAddrStart;
}

/* 内存管理模块初始化 */
void Mem_Init(void)
{
    put_str("Mem_Init start. \n");

    /* loader已经算得总物理内存大小 */
    uint32_t memTotalSize = (*(uint32_t *)MEM_TOTAL_SIZE_ADDR);
    
    put_str("memTotalSize: ");
    put_int(memTotalSize);
    put_str("\n");

    Mem_PoolInit(memTotalSize);

    put_str("Mem_Init end. \n");
}