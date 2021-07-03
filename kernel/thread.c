/*
 *  kernel/thread.h
 *
 *  (C) 2021  Jacky
 */

#include "thread.h"
#include "stdint.h"
#include "kernel/global.h"
#include "kernel/memory.h"
#include "kernel/panic.h"
#include "kernel/interrupt.h"
#include "kernel/process.h"
#include "kernel/sync.h"
#include "lib/string.h"
#include "lib/list.h"
#include "lib/print.h"

/* 就绪队列 */
List threadReadyList;

/* 所有任务队列 */
List threadAllList;

/* 主线程PCB */
Task *mainThreadTask;

void init(void);

/* 切换到下一个任务 */
void Thread_SwitchTo(Task *currTask, Task *nextTask);

/* 分配pid锁 */
static Lock g_pidLock;

/* 获取当前任务的PCB地址 */
Task *Thread_GetRunningTask(void)
{
    uint32_t esp = 0;
    __asm__ volatile("mov %%esp, %0" : "=g"(esp));

    return (Task *)(esp & 0xfffff000);
}

static void Thread_KernelStart(ThreadFunc threadFunc, void *threadArgs)
{
    /* 函数入参允许为NULL */
    ASSERT(threadFunc != NULL);

    /* 打开中断 */
    Idt_IntrEnable();

    threadFunc(threadArgs);
}

/* 申请任务标识符 */
static inline pid_t Thread_AllocPid(void)
{
    Lock_Lock(&g_pidLock);
    static pid_t nextPid = 0;
    nextPid ++;
    Lock_UnLock(&g_pidLock);
    return nextPid;
}

/* fork是返回pid */
pid_t Thread_ForkPid(void)
{
    return Thread_AllocPid();
}

/* 任务栈初始化 */
static inline void Thread_TaskInit(Task *task, const char *name, uint32_t priority, ThreadFunc threadFunc, void *threadArgs)
{
    /* PCB中的ebp指针默认指向该PCB的最大值 */
    task->taskStack = (uint32_t *)((uintptr_t)task + PAGE_SIZE);
    task->pid = Thread_AllocPid();
    strcpy(task->name, name);
    task->priority = priority;
    task->ticks = priority;
    task->elapsedTicks = 0;
    task->pgDir = NULL;
    /* 以根目录为默认工作路径 */
    task->cwdIndoe = 0;
    /* -1表示没有父进程 */
    task->parentPid = -1;
    task->stackMagic = 0x19AE1617;
    task->taskStatus = TASK_READY;

    task->fdTable[0] = 0;
    task->fdTable[1] = 1;
    task->fdTable[2] = 2;
    for (uint8_t i = 3; i < MAX_FILES_OPEN_PER_PROC; i++) {
        task->fdTable[i] = -1;
    }

    if (task == mainThreadTask) {
        /* 如果是main任务，其已经正在运行状态了 */
        task->taskStatus = TASK_RUNNING;
        /* main任务已经设置了相应的寄存器，无需重复设置 */
        return;
    }

    /* 中断栈放在PCB的最高处 */
    task->taskStack -= sizeof(IntrStack);
    /* 任务栈放在中断栈下面 */
    task->taskStack -= sizeof(TaskStack);
    TaskStack *taskStack = (TaskStack *)task->taskStack;

    /* 待保存寄存器初始化为0 */
    taskStack->ebp = 0;
    taskStack->ebx = 0;
    taskStack->edi = 0;
    taskStack->esi = 0;

    taskStack->eip = Thread_KernelStart;
    taskStack->threadFunc = threadFunc;
    taskStack->threadArgs = threadArgs;
}

/* 线程创建函数 */
Task *Thread_Create(const char *name, uint32_t priority, ThreadFunc threadFunc, void *threadArgs)
{
    /* 创建线程PCB空间，即1个页表 */
    Task *task = Mem_GetKernelPages(1);

    Thread_TaskInit(task, name, priority, threadFunc, threadArgs);

    /* 将线程节点存放到就绪队列和全部队列中 */
    ASSERT(List_Find(&threadAllList, &(task->threadListTag)) != true);
    List_Append(&threadAllList, &(task->threadListTag));

    ASSERT(List_Find(&threadReadyList, &(task->generalTag)) != true);
    List_Append(&threadReadyList, &(task->generalTag));

    return task;
}

/* 创建kernel的main线程，当前main线程的栈指针为0xc009f000，所以其PCB地址为0xc009e000 */
static void Thread_MakeMainThread(void)
{
    /* 由于loader中已经预留其PCB地址，这里无需分配一块内存页 */
    mainThreadTask = Thread_GetRunningTask();
    /* 必须保证main线程是第一个运行的任务，否则系统会崩溃 */
    Thread_TaskInit(mainThreadTask, "main", 31, NULL, NULL);
    
    /* 将main线程节点存放到全部队列中 */
    ASSERT(List_Find(&threadAllList, &(mainThreadTask->threadListTag)) != true);
    List_Append(&threadAllList, &(mainThreadTask->threadListTag));

    return;
}

/* 通过任务链表节点获取任务的PCB地址 */
Task *Thread_GetTaskPCB(const ListNode *listNode)
{
    return ELEM2ENTRY(Task, generalTag, listNode);
}

/* 任务调度 */
void Thread_Schedule(void)
{
    /* 获取当前的任务 */
    Task *currTask = Thread_GetRunningTask();
    if (currTask->taskStatus == TASK_RUNNING) {
        /* 时间片用完调度 */
        ASSERT(List_Find(&threadReadyList, &(currTask->generalTag)) != true);
        /* 重新将当前任务加入链表 */
        List_Append(&threadReadyList, &(currTask->generalTag));
        currTask->taskStatus = TASK_READY;
        currTask->ticks = currTask->priority;
    }

    /* 如果链表队列为空，表示当前只有一个main任务，直接返回 */
    if (List_IsEmpty(&threadReadyList) == true) {
        return;
    }

    /* 从链表中获取链表头节点，送上CPU运行 */
    ListNode *nextNode = List_Pop(&threadReadyList);
    /* 获取节点对应任务的PCB */
    Task *nextTask = Thread_GetTaskPCB(nextNode);
    nextTask->taskStatus = TASK_RUNNING;
    
    /* 激活下个任务的页表 */
    Process_Activate(nextTask);

    /* 任务切换 */
    Thread_SwitchTo(currTask, nextTask);

    return;
}

/* 任务主动让出cpu使用权 */
void Thread_Yield(void)
{
    Task *currTask = Thread_GetRunningTask();
    IntrStatus status = Idt_IntrDisable();
    currTask->taskStatus = TASK_READY;
    ASSERT(List_Find(&threadReadyList, &currTask->generalTag) == false);
    List_Append(&threadReadyList, &currTask->generalTag);
    Thread_Schedule();
    Idt_SetIntrStatus(status);

    return;
}

/* 当前任务阻塞 */
void Thread_Block(TaskStatus status)
{
    /* 任务阻塞只能是如下三种状态 */
    ASSERT((status == TASK_BLOCKED) || (status == TASK_WAITING) || (status == TASK_HANDING));
    IntrStatus oldStatus = Idt_IntrDisable();
    Task *currTask = Thread_GetRunningTask();
    currTask->taskStatus = status;
    /* 重新调度给其他任务 */
    Thread_Schedule();
    /* 解阻塞后，重新设置中断状态 */
    Idt_SetIntrStatus(oldStatus);
}

/* 当前任务被唤醒 */
void Thread_UnBlock(Task *task)
{
    ASSERT(task != NULL);
    IntrStatus oldStatus = Idt_IntrEnable();
    IntrStatus status = task->taskStatus;
    ASSERT((status == TASK_BLOCKED) || (status == TASK_WAITING) || (status == TASK_HANDING));
    if (task->taskStatus != TASK_READY) {
        /* 将任务添加到待运行队列 */
        ASSERT(List_Find(&threadReadyList, &task->generalTag) == false);
        List_Push(&threadReadyList, &task->generalTag);
        task->taskStatus = TASK_READY;
    }
    
    /* 解阻塞后，重新设置中断状态 */
    Idt_SetIntrStatus(oldStatus);
}

/* 任务初始化 */
void Thread_Init(void)
{
    put_str("Thread_Init start. \n");
    
    List_Init(&threadAllList);
    List_Init(&threadReadyList);
    
    /* 初始化pid锁 */
    Lock_Init(&g_pidLock);

    /* 先创建第一个用户进程：init */
    Process_Create(init, "init");

    /* 创建主线程PCB */
    Thread_MakeMainThread();
    
    put_str("Thread_Init end. \n");

    return;
}