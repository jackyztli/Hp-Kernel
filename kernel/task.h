#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>

/* 任务栈大小32K */
#define STACK_SIZE (32 * 1024)
#define STACK_BYTES (STACK_SIZE / sizeof(uint64_t))

/* 段选择子宏定义 */
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10

typedef uint64_t pml4t_t;

enum task_state
{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
};

enum task_flag
{
    TASK_KERNEL,
};

struct mm_struct
{
    /* 任务页目录基址，即CR3寄存器 */
    pml4t_t *pgd;

    /* 任务虚拟段 */
    uintptr_t start_code;
    uintptr_t end_code;
    uintptr_t start_data;
    uintptr_t end_code;
    uintptr_t start_rodata;
    uintptr_t end_rodate;
    uintptr_t start_brk;
    uintptr_t end_brk;
    uintptr_t start_stack;
};

struct thread_struct
{
    uintptr_t rsp0;

    uintptr_t rip;
    uintptr_t rsp;

    uintptr_t fs;
    uintptr_t gs;

    uintptr_t cr2;
    uint64_t trap_nr;
    uint64_t error_code;
};

/* 任务PCB */
struct task_struct
{
    /* 任务id */
    uint64_t pid;

    /* 任务的状态 */
    enum task_state state;
    
    /* 任务标识 */
    enum task_flag flag;

    /* 任务内存管理 */
    struct mm_struct *mm;
    
    /* 任务信息 */
    struct thread_struct *thread;

    /* 任务计时器 */
    uint64_t counter;

    /* 任务信号相关信息 */
    uint64_t signal;

    /* 任务优先级 */
    uint32_t priority;
};

/* 任务栈空间 */
union task_union
{
    struct task_struct task;
    uint64_t stack[STACK_BYTES];
}__attribute__((aligned(8)));

/* 任务初始化 */
void init_task(void);

#endif