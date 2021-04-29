/*
 *  kernel/process.c
 *
 *  (C) 2021  Jacky
 */

#include "process.h"
#include "kernel/global.h"
#include "kernel/memory.h"
#include "kernel/thread.h"
#include "kernel/console.h"
#include "kernel/panic.h"
#include "kernel/interrupt.h"
#include "kernel/tss.h"
#include "lib/string.h"

/* 进程初始化 */
void Process_Start(void *func)
{
    Task *curTask = Thread_GetRunningTask();
    /* 创建线程时，PCB最高处为中断栈，紧接着是任务栈，此处taskStack指向initStack的地方 */
    curTask->taskStack += sizeof(TaskStack);
    IntrStack *initStack = (IntrStack *)curTask->taskStack;
    /* 初始化中断栈的寄存器 */
    initStack->edi = 0;
    initStack->esi = 0;
    initStack->ebp = 0;
    initStack->espDummy = 0;
    initStack->ebx = 0;
    initStack->edx = 0;
    initStack->ecx = 0;
    initStack->eax = 0;
    /* 特权级为3的显示选择子段为0，不可直接访问显存 */
    initStack->gs = 0;
    /* 代码段选择子 */
    initStack->ds = SELECTOR_U_DATA;
    initStack->es = SELECTOR_U_DATA;
    initStack->fs = SELECTOR_U_DATA;
    /* 数据段选择子 */
    initStack->cs = SELECTOR_U_CODE;
    /* eflags寄存器 */
    initStack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);

    initStack->eip = func;
    initStack->esp = (void *)((uintptr_t)Mem_GetOnePage(VIR_MEM_USER, USER_VADDR_STACK - PAGE_SIZE) + PAGE_SIZE);
    initStack->ss = SELECTOR_U_STACK;

    /* 从中断处返回，退到低特权级 */
    __asm__ volatile ("movl %0, %%esp; jmp intr_exit" : : "g"(initStack) : "memory");
}

/* 创建进程页目录表 */
uint32_t *Process_PageDir(void)
{
    /* 进程页目录表存放在内核空间 */
    uint32_t *pageDirVAddr = Mem_GetKernelPages(1);
    if (pageDirVAddr == NULL) {
        Console_PutStr("Process_PageDir Failed!");
        return NULL;
    }

    /* 复制内核页目录项（768(0x300)页往上项），使所有进程共享内核页表 
     * 其中0xfffff000表示获取到内核页目录表物理地址的虚拟地址，复制256项
    */
    memcpy((uint32_t *)((uintptr_t)pageDirVAddr + 0x300 * 4), (uint32_t *)(0xfffff000 + 0x300 * 4), 256 * 4);

    /* 更新页目录项最后一项的内容，最后一项默认指向页目录表开头 */
    uintptr_t pageDirPhyAddr = Mem_V2P((uintptr_t)pageDirVAddr);
    pageDirVAddr[1023] = pageDirPhyAddr | PG_US_U | PG_RW_W | PG_P_1;

    return pageDirVAddr;
}

/* 创建用户进程虚拟地址位图 */
void Process_VAddrBitmap(Task *task)
{
    /* 用户进程虚拟内存池起始地址 */
    task->progVaddrPool.virtualAddrStart = USER_VADDR_START;

    /* 计算bitmap最少需要多少页 */
    uint32_t bitmapPgCnt = DIV_ROUND_UP((0xc00000000 - USER_VADDR_START) / PAGE_SIZE / 8, PAGE_SIZE);
    task->progVaddrPool.bitmap.bitmap = Mem_GetKernelPages(bitmapPgCnt);
    task->progVaddrPool.bitmap.bitmapLen = (0xc00000000 - USER_VADDR_START) / PAGE_SIZE / 8;
    /* 初始化虚拟地址内存池 */
    BitmapInit(&task->progVaddrPool.bitmap);
}

/* 激活线程或进程页表 */
void Process_Activate(Task *task)
{
    ASSERT(task != NULL);

    /* 内核线程同样要激活页表，否则上次运行的是进程，此处没有激活页表，那么将会使用错误的页表 */
    uint32_t pageDirPhyAddr = 0x100000;
    if (task->pgDir != NULL) {
        /* 如果是要激活进程页表 */
        pageDirPhyAddr = Mem_V2P((uintptr_t)task->pgDir);       
    }

    __asm__ volatile ("movl %0, %%cr3" : : "r"(pageDirPhyAddr) : "memory");

    if (task->pgDir != NULL) {
        TSS_UpdateEsp(task);
    }

    return;
}

/* 进程创建 */
void Process_Create(void *fileName, char *name)
{
    /* 1. 关中断 */
    IntrStatus status = Idt_IntrDisable();

    /* 2. 创建进程的主线程，进程启动为执行Process_Start(fileName)，后续通过Thread_Schedule函数切换 */
    Task *task = Thread_Create(name, 31, Process_Start, fileName);

    /* 3. 创建也目录表 */
    task->pgDir = Process_PageDir();

    /* 4. 初始化虚拟内存池 */
    Process_VAddrBitmap(task);

    /* 5. 初始化进程私有的内存描述符数组 */
    Mem_BlockDescInit(task->memblockDesc);

    /* 6. 设置中断状态 */
    Idt_SetIntrStatus(status);

    return;
}