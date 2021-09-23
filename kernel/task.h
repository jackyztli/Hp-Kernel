#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>
#include <list.h>

/* 任务栈大小32K */
#define STACK_SIZE (32 * 1024)
#define STACK_BYTES (STACK_SIZE / sizeof(uint64_t))

/* 段选择子宏定义 */
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS 0x28
#define USER_DS 0x30

typedef uint64_t pml4t_t;
typedef uint64_t pid_t;

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
    TASK_USER,
};

struct mm_struct
{
    /* 任务页目录基址，即CR3寄存器 */
    pml4t_t *pgd;

    /* 任务虚拟段 */
    uintptr_t start_code;
    uintptr_t end_code;
    uintptr_t start_data;
    uintptr_t end_data;
    uintptr_t start_rodata;
    uintptr_t end_rodate;
    uintptr_t start_brk;
    uintptr_t end_brk;
    uintptr_t start_stack;
};

/* 任务初始化栈，在首次调用时使用，存放在任务栈最高处 */
struct pt_regs
{
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rbp;
	uint64_t ds;
	uint64_t es;
	uint64_t rax;
	uint64_t func;
	uint64_t errcode;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

struct thread_struct
{
    uintptr_t rsp0;

    uintptr_t rip;
    uintptr_t rsp;
    /* 在首次调用switch_to获得cpu资源时，rip指向启动start_kernel_func函数；否则rip指向switch_to的返回值 
     * rsp首次调用指向任务私有栈开始处，在首次执行启动函数时，该值会赋给rbp寄存器
    */

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
    pid_t pid;

    struct list task_node;

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

/* TSS定义 */
struct tss_struct
{
	uint32_t reserved0;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t reserved1;
	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;
	uint64_t reserved2;
	uint16_t reserved3;
	uint16_t iomapbaseaddr;
}__attribute__((packed));

static inline struct task_struct *get_current(void)
{
    struct task_struct *current = NULL;
    __asm__ volatile("andq %%rsp, %0" :"=r"(current) :"0"(~32767UL));
    return current;
}

#define current get_current()

/* 任务初始化 */
void setup_task(void);
/* tss初始化 */
void init_tss(void);

/* 创建任务入口 */
pid_t create_task(uint64_t (* func)(void *), void *args);

/* 任务调度 */
void schedule(void);

/* 创建进程 */
pid_t do_fork(const struct pt_regs *regs);

#endif