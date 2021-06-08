/*
 *  kernel/memory.c
 *
 *  (C) 2021  Jacky
 */

#include "memory.h"
#include "stdint.h"
#include "kernel/panic.h"
#include "kernel/bitmap.h"
#include "kernel/sync.h"
#include "kernel/interrupt.h"
#include "kernel/global.h"
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

/* 物理内存池 */
typedef struct {
    Bitmap bitmap;
    uint32_t phyAddrStart;
    uint32_t poolSize;
    Lock memLock;
} MemPool;

/* 内核物理内存池 */
MemPool kernelMemPool;
/* 用户物理内存池 */
MemPool userMemPool;

/* 内核内存块描述符数组 */
static MemBlockDesc kernelBlockDesc[DESC_CNT];

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
    /* 初始化内存池锁 */
    Lock_Init(&kernelMemPool.memLock);
    
    /* 初始化用户物理内存池 */
    userMemPool.bitmap.bitmapLen = userFreePages / 8;
    userMemPool.bitmap.bitmap = (uint8_t *)(MEM_BITMAP_BASE + kernelMemPool.bitmap.bitmapLen);
    userMemPool.phyAddrStart = kernelMemPool.phyAddrStart + kernelMemPool.poolSize;
    userMemPool.poolSize = userFreePages * PAGE_SIZE;
    /* 将位图置0，表示内存未被使用 */
    BitmapInit(&userMemPool.bitmap);
    /* 初始化内存池锁 */
    Lock_Init(&userMemPool.memLock);

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

static inline uintptr_t Mem_GetVirAddrFromBitmap(Bitmap *bitmap, uint32_t pagesNums, uint32_t virtualAddrStart)
{
    /* 通过位图找到合适的空间 */
    int32_t startBitIndex = BitmapScan(bitmap, pagesNums);
    /* 找不到n个可用的连续虚拟页 */
    if (startBitIndex == -1) {
        return NULL;
    }

    /* 从startBitIndex开始，将pagesNums位bit置1，表示已被占用 */
    for (uint32_t i = 0; i < pagesNums; i++) {
        BitmapSet(&kernelVirMemPool.bitmap, startBitIndex + i, 1);
    }

    return virtualAddrStart + startBitIndex * PAGE_SIZE;
}

/* 分配n个连续虚拟页，成功返回虚拟地址，失败者返回NULL */
static void *Mem_GetVirAddr(VirMemType virMemtype, uint32_t pagesNums)
{
    uintptr_t virAddr = NULL;

    if (virMemtype == VIR_MEM_KERNEL) {
        virAddr = Mem_GetVirAddrFromBitmap(&kernelVirMemPool.bitmap, pagesNums, kernelVirMemPool.virtualAddrStart);
    } else {
        /* 分配用户虚拟地址 */
        Task *currTask = Thread_GetRunningTask();
        virAddr = Mem_GetVirAddrFromBitmap(&currTask->progVaddrPool.bitmap, pagesNums, currTask->progVaddrPool.virtualAddrStart);
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
        /* 页目录项不存在，先创建页表映射，用户进程的页目录表也存放在内核内存池中 */
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

/* 申请一页空间，并映射到指定地址 */
void *Mem_GetOnePage(VirMemType virMemType, uintptr_t virAddrStart)
{
    MemPool *memPool = NULL;
    if (virMemType == VIR_MEM_KERNEL) {
        memPool = &kernelMemPool;
    } else if (virMemType == VIR_MEM_USER) {
        memPool = &userMemPool;
    }

    Lock_Lock(&memPool->memLock); 

    Task *curTask = Thread_GetRunningTask();
    int32_t bitIndex = -1;
    /* 用户进程修改用户进程自己虚拟内存池 */
    if ((curTask->pgDir != NULL) && (virMemType == VIR_MEM_USER)) {
        bitIndex = ((uintptr_t)virAddrStart - curTask->progVaddrPool.virtualAddrStart) / PAGE_SIZE;
        ASSERT(bitIndex > 0);
        BitmapSet(&curTask->progVaddrPool.bitmap, bitIndex, 1);
    } else if ((curTask->pgDir == NULL) && (virMemType == VIR_MEM_KERNEL)) {
        /* 内核线程修改内核虚拟内存池 */
        bitIndex = ((uintptr_t)virAddrStart - kernelVirMemPool.virtualAddrStart) / PAGE_SIZE;
        ASSERT(bitIndex > 0);
        BitmapSet(&kernelVirMemPool.bitmap, bitIndex, 1);
    } else {
        /* 出错 */
        PANIC("GetOnePage Failed!");
    }

    /* 申请一页物理内存 */
    void *pagePhyAddr = Mem_Palloc(memPool);
     if (pagePhyAddr == NULL) {
        Lock_UnLock(&memPool->memLock);
        return NULL;
    }

    Mem_AddPageTable((void *)virAddrStart, pagePhyAddr);

    Lock_UnLock(&memPool->memLock);

    return (void *)virAddrStart;
}

/* 用户进程申请n个页空间 */
void *Mem_GetUserPages(uint32_t pageNum)
{
    Lock_Lock(&userMemPool.memLock);
    void *virAddrStart = Mem_MallocPages(VIR_MEM_USER, pageNum);
    if (virAddrStart == NULL) {
        Lock_UnLock(&userMemPool.memLock);
        return NULL;
    }

    memset(virAddrStart, 0, pageNum * PAGE_SIZE);

    Lock_UnLock(&userMemPool.memLock);

    return virAddrStart;
}

/* 内核申请n个页空间 */
void *Mem_GetKernelPages(uint32_t pageNum)
{
    Lock_Lock(&kernelMemPool.memLock);
    void *virAddrStart = Mem_MallocPages(VIR_MEM_KERNEL, pageNum);
    if (virAddrStart == NULL) {
        Lock_UnLock(&kernelMemPool.memLock);
        return NULL;
    }

    memset(virAddrStart, 0, pageNum * PAGE_SIZE);

    Lock_UnLock(&kernelMemPool.memLock);
    return virAddrStart;
}

/* 根据虚拟地址获取对应物理地址 */
uintptr_t Mem_V2P(uintptr_t virAddr)
{
    /* 获取virAddr对应的页表物理地址 */
    uint32_t *pte = Mem_GetVirAddrPtePtr(virAddr);
    return (uintptr_t)((*pte & 0xfffff000) + (virAddr & 0x00000fff));
}

/* 初始化内核内存块描述符数组 */
void Mem_BlockDescInit(MemBlockDesc *memBlockDesc)
{
    uint32_t blockSize = 16;
    for (uint8_t i = 0; i < DESC_CNT; i++) {
        /* 每个内存块大小 */
        memBlockDesc[i].blockSize = blockSize;
        memBlockDesc[i].blockPerArena = (PAGE_SIZE - sizeof(MemArena)) / blockSize;
        List_Init(&memBlockDesc[i].freeList);

        blockSize *= 2;
    }
}

/* 获取arena中的第n个内存块 */
static inline MemBlock *Mem_Arena2Block(const MemArena *arena, uint32_t n)
{
    return (MemBlock *)((uintptr_t)arena + sizeof(MemArena) + n * arena->desc->blockSize);
}

/* 获取arena中的第n个内存块 */
static inline MemArena *Mem_Block2Arena(const MemBlock *memBlock)
{
    return (MemArena *)((uintptr_t)memBlock & 0xfffff000);
}

/* 解除虚拟地址页和物理地址的映射关系 */
static void inline Mem_PageTableRemove(uintptr_t virAddr)
{
    uint32_t *pte = Mem_GetVirAddrPtePtr(virAddr);
    /* 将P位置0，表示不可访问 */
    *pte &= ~PG_P_1;
    __asm__ volatile ("invlpg %0" : : "m"(virAddr) : "memory");

    return;
}

/* 将n个虚拟地址页回收 */
void Mem_FreeVirAddr(VirMemType type, void *virAddr, uint32_t pageCnt)
{
    uint32_t cnt = 0;
    uint32_t bitIndex;
    if (type == VIR_MEM_KERNEL) {
        bitIndex = ((uintptr_t)virAddr - kernelVirMemPool.virtualAddrStart) / PAGE_SIZE;
        while (cnt < pageCnt) {
            // ASSERT(BitmapGet(&kernelVirMemPool.bitmap, bitIndex) == 1);
            BitmapSet(&kernelVirMemPool.bitmap, bitIndex + cnt, 0);
            Mem_PageTableRemove((uintptr_t)virAddr + cnt * PAGE_SIZE);
            cnt++;
        }
    } else if (type == VIR_MEM_USER) {
        Task *currTask = Thread_GetRunningTask();
        bitIndex = ((uintptr_t)virAddr - currTask->progVaddrPool.virtualAddrStart) / PAGE_SIZE;
        while (cnt < pageCnt) {
            // ASSERT(BitmapGet(&currTask->progVaddrPool.bitmap, bitIndex) == 1);
            BitmapSet(&currTask->progVaddrPool.bitmap, bitIndex + cnt, 0);
            Mem_PageTableRemove((uintptr_t)virAddr + cnt * PAGE_SIZE);
            cnt++;
        }
    }

    return;
}

/* 将物理页回收 */
void Mem_FreePhyAddr(uintptr_t phyAddr)
{
    uint32_t bitIndex;
    MemPool *memPool = NULL;
    if (phyAddr >= userMemPool.poolSize) {
        /* 释放用户物理内存池 */
        memPool = &userMemPool;
        bitIndex = (phyAddr - userMemPool.phyAddrStart) / PAGE_SIZE;
    } else {
        memPool = &kernelMemPool;
        bitIndex = (phyAddr - kernelMemPool.phyAddrStart) / PAGE_SIZE;
    }

    /* 对应的图位应该为1，只能释放已经分配的物理页 */
    ASSERT(BitmapGet(&memPool->bitmap, bitIndex) == 1);
    /* 将内存池对应位清0 */
    BitmapSet(&memPool->bitmap, bitIndex, 0);

    return;
}

/* 释放虚拟地址virAddr的n个页面 */
void Mem_Free(VirMemType type, void *virAddr, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        /* 获取虚拟地址对应的物理地址 */
        uintptr_t phyAddr = Mem_V2P((uintptr_t)virAddr + i * PAGE_SIZE);
        Mem_FreePhyAddr(phyAddr);
    }

    /* 释放虚拟地址 */
    Mem_FreeVirAddr(type, virAddr, n);

    return;
}

/* 在堆上申请size大小字节内存 */
void *Mem_Malloc(uint32_t size)
{
    VirMemType virMemType; 
    MemPool *memPool = NULL;
    MemBlockDesc *memBlockDesc = NULL;
    /* 获取当前正在执行的任务 */
    Task *currTask = Thread_GetRunningTask();
    if (currTask->pgDir == NULL) {
        /* 内核线程申请内存 */
        virMemType = VIR_MEM_KERNEL;
        memPool = &kernelMemPool;
        memBlockDesc = kernelBlockDesc;
    } else {
        /* 用户进程申请内存 */
        virMemType = VIR_MEM_USER;
        memPool = &userMemPool;
        memBlockDesc = currTask->memblockDesc;
    }

    /* 判断是否有足够则空间 */
    if (size >= memPool->poolSize) {
        return NULL;
    }

    Lock_Lock(&memPool->memLock);
    MemArena *arena = NULL;
    /* 如果分配的是大内存（超过1024字节，则整个页框分配）*/
    if (size > 1024) {
        uint32_t pageCnt = DIV_ROUND_UP(size + sizeof(MemArena), PAGE_SIZE);
        /* 申请n个物理页框，并返回对应的虚拟地址 */
        arena = Mem_MallocPages(virMemType, pageCnt);
        if (arena == NULL) {
            Lock_UnLock(&memPool->memLock);
            return NULL;
        }

        /* 超过1024字节的内存块没有描述符 */
        arena->desc = NULL;
        arena->cnt = pageCnt;
        arena->large = true;
        Lock_UnLock(&memPool->memLock);
        /* 跳过开头的存放arena的空间，后面才是真正分配使用的内存 */
        return (void *)(arena + 1);
    } else {
        /* 分配小内存，先找到对应描述符 */
        uint8_t idx = 0;;
        while ((idx < DESC_CNT) && (memBlockDesc[idx].blockSize < size)) {
            idx++;
        }

        ASSERT(idx < DESC_CNT);

        /* 先判断对应的描述符数组中是否还有空闲块 */
        if (List_IsEmpty(&memBlockDesc[idx].freeList)) {
            /* 创建可用的内存空间 */
            arena = Mem_MallocPages(virMemType, 1);
            if (arena == NULL) {
                Lock_UnLock(&memPool->memLock);
                return NULL;
            }

            /* 将新创建的内存块划分成多个小块 */
            arena->desc = &memBlockDesc[idx];
            arena->cnt = memBlockDesc[idx].blockPerArena;
            arena->large = false;

            /* 关中断，将内存块描述符添加到freelist中 */
            IntrStatus status = Idt_IntrDisable();
            
            for (uint8_t i = 0; i < arena->cnt; i++) {
                MemBlock *memBlock = Mem_Arena2Block(arena, i);
                ASSERT(List_Find(&arena->desc->freeList, &memBlock->freeNode) == false);
                List_Append(&arena->desc->freeList, &memBlock->freeNode);
            }

            Idt_SetIntrStatus(status);
        }

        /* 取空闲列表头节点，分配使用 */
        MemBlock *memBlock = (MemBlock *)List_Pop(&memBlockDesc[idx].freeList);
        /* 所在的arena块空闲数量减1 */
        arena = Mem_Block2Arena(memBlock);
        arena->cnt--;
        Lock_UnLock(&memPool->memLock);
        return (void *)memBlock;
    }
}

/* 安装1页大小的vaddr，在fork场景使用 */
void *Mem_GetPageWithoutOpBitmap(VirMemType type, uintptr_t vaddr)
{
    MemPool *memPool = type == VIR_MEM_KERNEL ? &kernelMemPool : &userMemPool;
    Lock_Lock(&memPool->memLock);
    void *pagePhyAddr = Mem_Palloc(memPool);
    if (pagePhyAddr == NULL) {
        Lock_UnLock(&memPool->memLock);
        return NULL;
    }

    Mem_AddPageTable((void *)vaddr, pagePhyAddr);
    Lock_UnLock(&memPool->memLock);

    return (void *)vaddr;
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

    /* 初始化内核内存块描述符数组 */
    Mem_BlockDescInit(kernelBlockDesc);

    put_str("Mem_Init end. \n");
}

/* 系统调用相关函数实现 */

void *sys_malloc(uint32_t size)
{
    return Mem_Malloc(size);
}

void sys_free(void *addr)
{
    ASSERT(addr != NULL);
    if (addr == NULL) {
        return;
    }

    VirMemType type;
    MemPool *memPool = NULL;
    /* 通过判断是线程还是进程，获取到虚拟地址的类型 */
    if (Thread_GetRunningTask()->pgDir == NULL) {
        /* 内核线程 */
        type = VIR_MEM_KERNEL;
        memPool = &kernelMemPool;
    } else {
        type = VIR_MEM_USER;
        memPool = &userMemPool;
    }

    Lock_Lock(&memPool->memLock);
    MemBlock *memBlock = (MemBlock *)addr;
    MemArena *arena = Mem_Block2Arena(memBlock);
    if (arena->large == true) {
        /* 大于1024字节空间的内存块 */
        Mem_Free(type, (void *)arena, arena->cnt);
    } else {
        /* 小于或等于1024字节内存块回收 */
        List_Append(&arena->desc->freeList, &memBlock->freeNode);

        /* 只有当整块arena空间都空闲才回收页框 */
        arena->cnt++;
        if (arena->cnt == arena->desc->blockPerArena) {
            for (uint32_t blockIndex = 0; blockIndex < arena->desc->blockPerArena; blockIndex++) {
                MemBlock *memBlock = Mem_Arena2Block(arena, blockIndex);
                ASSERT(List_Find(&arena->desc->freeList, &memBlock->freeNode) == true);
                List_Remove(&memBlock->freeNode);
            }

            Mem_Free(type, (void *)arena, 1);
        }
    }
    
    Lock_UnLock(&memPool->memLock);

    return;
}
