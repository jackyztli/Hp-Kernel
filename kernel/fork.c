/*
 *  kernel/fork.c
 *
 *  (C) 2021  Jacky
 */

#include "stdint.h"
#include "kernel/thread.h"
#include "kernel/global.h"
#include "kernel/panic.h"
#include "kernel/process.h"
#include "kernel/interrupt.h"
#include "lib/string.h"
#include "fs/file.h"

/* 将父进程的pcb拷贝到子进程中 */
static int32_t Fork_CopyPCBToChild(const Task *parent, Task *child)
{
    /* 1、拷贝整个PCB页空间 */
    memcpy(child, parent, PAGE_SIZE);
    child->pid = Thread_ForkPid();
    child->elapsedTicks = 0;
    child->taskStatus = TASK_READY;
    child->ticks = child->priority;
    child->parentPid = parent->parentPid;
    child->generalTag.prev = NULL;
    child->generalTag.next = NULL;
    child->threadListTag.prev = NULL;
    child->threadListTag.next = NULL;

    /* 2、复制父进程的虚拟地址池位图，需要新申请页表，否则和父进程使用同样页表 */
    Mem_BlockDescInit(child->memblockDesc);
    uint32_t bitmapPageCnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PAGE_SIZE / 8, PAGE_SIZE);
    void *vaddrBitmap = Mem_GetKernelPages(bitmapPageCnt);
    memcpy(vaddrBitmap, child->progVaddrPool.bitmap.bitmap, bitmapPageCnt * PAGE_SIZE);
    child->progVaddrPool.bitmap.bitmap = vaddrBitmap;
    ASSERT(strlen(child->name) < 11);
    strcat(child->name, "_fork");

    return 0;
}

/* 复制子进程的进程体和用户栈 */
static void Fork_CopyUserStack(Task *parent, Task *child, void *buf)
{
    uint8_t *vaddrBitmap = parent->progVaddrPool.bitmap.bitmap;
    uint32_t bitmapByteLen = parent->progVaddrPool.bitmap.bitmapLen;
    uint32_t vaddrStart = parent->progVaddrPool.virtualAddrStart;
    uint32_t idxByte = 0;

    while (idxByte < bitmapByteLen) {
        if (vaddrBitmap[idxByte]) {
            uint32_t idxBit = 0;
            while (idxBit < 8) {
                uint8_t bitMask = (1 << idxBit);
                if (bitMask & vaddrBitmap[idxByte]) {
                    uintptr_t progVaddr = vaddrStart + (idxByte * 8 + idxBit) * PAGE_SIZE;
                    /* 将父进程用户空间的数据通过内核空间中转到用户空间 */
                    /* 1、拷贝页到内核空间 */
                    memcpy(buf, (void *)progVaddr, PAGE_SIZE);

                    /* 2、页表切换到子进程，这样下面申请的页表项才能安装到子进程 */
                    Process_Activate(child);

                    /* 3、申请虚拟地址 */
                    Mem_GetPageWithoutOpBitmap(VIR_MEM_USER, progVaddr);

                    /* 4、从内核中拷贝数据到子进程的用户空间 */
                    memcpy((void *)progVaddr, buf, PAGE_SIZE);

                    /* 5、恢复父进程页表 */
                    Process_Activate(parent);
                }
                idxBit++;
            }
        }
        idxByte++;
    }
}

/* 为子进程构建中断栈 */
static int32_t Fork_BuildChildStack(Task *child)
{
    /* 1、使得子进程的pid返回值为0 */
    IntrStack *intrStack = (IntrStack *)((uintptr_t)child + PAGE_SIZE - sizeof(IntrStack));
    /* 返回值设为0 */
    intrStack->eax = 0;

    /* 2、为switch to构建栈空间 */
    uint32_t *retAddr = (uint32_t *)intrStack - 1;
    *retAddr = intr_exit;

    /* intrStack - 5正好是ebp的地址 */
    child->taskStack = (uint32_t *)intrStack - 5;

    return 0;
}

/* 更新inode的打开数 */
static void Fork_UpdateInodeOpenCnt(Task *task)
{
    int32_t localFd = 3;
    while (localFd < MAX_FILES_OPEN_PER_PROC) {
        int32_t globalFd = task->fdTable[localFd];
        ASSERT(globalFd < MAX_FILE_OPEN);
        if (globalFd != -1) {
            g_fileTable[globalFd].fdInode->iOpenCnts++;
        }
        localFd++;
    }

    return;
}

/* 拷贝父进程本身所占用的资源给子进程 */
static int32_t Fork_CopyProcess(Task *parent, Task *child)
{
    void *buf = Mem_GetKernelPages(1);
    if (buf == NULL) {
        return -1;
    }

    /* 1、拷贝PCB、虚拟地址位图、内核栈 */
    if (Fork_CopyPCBToChild(parent, child) == -1) {
        return -1;
    }

    /* 2、为子进程创建页表 */
    child->pgDir = Process_PageDir();
    if (child->pgDir == NULL) {
        return -1;
    }

    /* 3、复制父进程进程体和用户栈给子进程 */
    Fork_CopyUserStack(parent, child, buf);

    /* 4、构建子进程的中断栈 */
    Fork_BuildChildStack(child);

    /* 5、更新文件打开的inode数 */
    Fork_UpdateInodeOpenCnt(child);

    Mem_FreeVirAddr(VIR_MEM_KERNEL, buf, 1);

    return 0;
}

/* fork子进程 */
pid_t sys_fork(void)
{
    Task *parent = Thread_GetRunningTask();
    Task *child = Mem_GetKernelPages(1);
    if (child == NULL) {
        return -1;
    }

    ASSERT((Idt_GetIntrStatus() == INTR_OFF) && (parent->pgDir != NULL));

    if (Fork_CopyProcess(parent, child) == -1) {
        return -1;
    }

    /* 子进程添加到就绪队列和所有线程队列 */
    ASSERT(List_Find(&threadAllList, &(child->threadListTag)) != true);
    List_Append(&threadAllList, &(child->threadListTag));
    ASSERT(List_Find(&threadReadyList, &(child->generalTag)) != true);
    List_Append(&threadReadyList, &(child->generalTag));

    /* 父进程返回子进程的pid */
    return child->pid;
}
