#include <task.h>
#include <stdint.h>
#include <memory.h>
#include <printk.h>
#include <system.h>
#include <syscall.h>
#include <list.h>

/* init任务，当前只支持单CPU，后续适配多CPU */
union task_union init_task;

struct list *ready_task_list;

#define TSS_SECTOR 8
static struct tss_struct tss;

/* 内核启动以来，系统总滴答数 */
static uint64_t ticks = 0;

#define INIT_TASK_CR3 0x0

#define _text _etext
extern char _etext;
#define _data _etext
extern char _edata;
#define _rodata _etext
#define _erodata _etext
extern char _end;

static inline pid_t alloc_task_pid(void)
{
    static pid_t pid = 0;
    return pid++;
}

/* 任务切换函数 */
static void switch_to(struct task_struct *cur, struct task_struct *next)
{
    __asm__ __volatile__(
        "movq  %%rsp, %0;        \
         movq  %1,    %%rsp;"
        :"=m"(cur->thread->rsp) :"m"(next->thread->rsp)
    );
}

/* 任务调度 */
void schedule(void)
{
    ticks++;
    if (current->counter != 0) {
        current->counter--;
        return;
    }

    if (list_empty(ready_task_list)) {
        current->counter = current->priority;
        return;
    }

    /* 任务到期，切换到其他任务 */
    current->counter = current->priority;
    add_list_tail(ready_task_list, &current->task_node);

    /* 取出链表头节点 */
    const struct list *next_thread_node = list_pop(ready_task_list);
    /* 转化为下一个节点的task_struct */
    struct task_struct *next = elem2entry(struct task_struct, task_node, next_thread_node);

    switch_to(current, next);
}

uint64_t do_exit(uint64_t code)
{
	printk("exit task is running, arg:%ld\n", code);
	while(1) {

    }
}

/* 内核线程执行入口 */
void kernel_thread_start(void)
{
    __asm__ volatile (
        "popq %15;"                \
        "popq %14;"                \
        "popq %13;"                \
        "popq %12;"                \
        "popq %11;"                \
        "popq %10;"                \
        "popq %9;"                 \
        "popq %8;"                 \
        "popq %rbx;"               \
        "popq %rcx;"               \
        "popq %rdx;"               \
        "popq %rsi;"               \
        "popq %rdi;"               \
        "popq %rbp;"               \
        "popq %rax;"               \
        "movq %rax, %ds;"          \
        "popq %rax;"               \
        "movq %rax, %es;"          \
        "popq %rax;"               \
        "movq $0x38, %rsp;"        \
        "callq *%rbx;"             \
        "movq %rax, %rdi;"         \
        "callq do_exit;"
    );
}

/* 创建任务入口 */
pid_t do_fork(const struct pt_regs *regs)
{
    uintptr_t page = PhyToVir(alloc_page());
    if (page == NULL) {
        printk("no enough memory!!!");
        return -1;
    } 

    union task_union *new_union = (union task_union *)page;
    memcpy(new_union, current, sizeof(task_union));
    struct task_struct *new_task = &new_union->task;
    new_task->pid = alloc_task_pid();
    new_task->task_node.next = NULL;
    new_task->state = TASK_RUNNING;
    new_task->flag = TASK_USER;

    new_task->mm = (struct mm_struct *)((uintptr_t)new_task + sizeof(struct task_struct));
    new_task->thread = (struct thread_struct *)((uintptr_t)new_task->mm + sizeof(struct mm_struct));
    new_task->thread->rsp0 = (uintptr_t)new_task + STACK_BYTES;
    new_task->thread->rip = (uintptr_t)ret_system_call;
    new_task->thread->rsp = (uintptr_t)new_task + STACK_BYTES - sizeof(struct pt_regs);
    new_task->thread->fs = USER_DS;
    new_task->thread->gs = USER_DS;
    new_task->thread->cr2 = 0;
    new_task->thread->trap_nr = 0;
    new_task->thread->error_code = 0;

    new_task->counter = new_task->priority;
    new_task->signal = 0;

    add_list_tail(ready_task_list, &new_task->task_node);

    /* 父进程返回子进程pid，子进程返回0 */
    struct pt_regs *new_regs = (struct pt_regs *)new_task->thread->rsp;
    new_regs->rax = 0;
    
    return new_task->pid;
}

/* 创建tss段描述符 */
void init_tss(void)
{
    /* 在64位系统上，TSS段描述符占16个字节，其结构如下：
     *  127                                         96                                          64
     *  -----------------------------------------------------------------------------------------
     *  |                   预留                     |               TSS地址高32位               |
     *  -----------------------------------------------------------------------------------------
     *  63                                                          31            15            0
     *  -----------------------------------------------------------------------------------------
     *  |基础地址31:24|G|0|0|AVL|段界限19:16|P|DPL|0|10B1|基础地址23:16| 基础地址15:00| 段界限低16位|
     *  -----------------------------------------------------------------------------------------
     *  属性值说明：
     *      P：表示是否在位
     *      DPL：特权级，x86中内核是0特权级，用户态是3特权级
     *      B：忙标志位
     *      G：粒度
     */
    /* 段描述符低64位 */
    uint64_t low_tss_desc = 0;
    /* 段界限15:00 */
    low_tss_desc += (sizeof(tss) & 0xffff);
    /* 基础地址15:00 */
    low_tss_desc += (((uintptr_t)&tss & 0xffff) << 16);
    /* 基础地址23:16 */
    low_tss_desc += (((uintptr_t)&tss & 0xff0000) << 32);
    /* 属性值 */
    low_tss_desc += (0x8b << 40);
    /* 段界限19:16 */
    low_tss_desc += ((sizeof(tss) & 0x0f0000) << 48);
    /* 属性值 */
    low_tss_desc += (8 << 52);
    /* 基础地址31:24 */
    low_tss_desc += (((uintptr_t)&tss & 0xff000000) << 56);

    /* 段描述符高64位 */
    uint64_t high_tss_desc = 0;
    high_tss_desc += ((uintptr_t)&tss & 0xffffffff);

    *(uint64_t *)gdt64[TSS_SECTOR] = low_tss_desc;
    *((uint64_t *)gdt64[TSS_SECTOR] + 1) = high_tss_desc;

    /* 加载TSS选择子 */
    load_tr(TSS_SECTOR);
}

/* 任务初始化 */
void setup_task(void)
{
    /* 说明：
     * 每一个任务栈占32K的空间，低地址存放PCB(task_struct)信息，紧接着是内存管理(mm_struct)信息，再接着是线程相关(thread_struct)信息
     * 任务栈从高到低存放
     */
    struct task_struct *p_task = &init_task.task;
    p_task->pid = alloc_task_pid();
    p_task->task_node.next = NULL;
    p_task->state = TASK_RUNNING;
    p_task->flag = TASK_KERNEL;

    /* 初始化内存管理相关信息 */
    p_task->mm = (struct mm_struct *)((uintptr_t)p_task + sizeof(struct task_struct));
    p_task->mm->pgd = (uintptr_t)INIT_TASK_CR3;
    p_task->mm->start_code = (uintptr_t)&_text;
    p_task->mm->end_code = (uintptr_t)&_etext;
    p_task->mm->start_data = (uintptr_t)&_data;
    p_task->mm->end_data = (uintptr_t)&_edata;
    p_task->mm->start_rodata = (uintptr_t)&_rodata;
    p_task->mm->end_code = (uintptr_t)&_erodata;
    p_task->mm->start_brk = (uintptr_t)0;
    p_task->mm->end_brk = (uintptr_t)&_end;
    p_task->mm->start_stack = (uintptr_t)p_task + STACK_BYTES;

    /* 初始化线程相关信息 */
    p_task->thread = (struct thread_struct *)((uintptr_t)p_task->mm + sizeof(struct mm_struct));
    p_task->thread->rsp0 = (uintptr_t)p_task + STACK_BYTES;
    p_task->thread->rsp = (uintptr_t)p_task + STACK_BYTES;
    p_task->thread->fs = KERNEL_DS;
    p_task->thread->gs = KERNEL_DS;
    p_task->thread->cr2 = 0;
    p_task->thread->trap_nr = 0;
    p_task->thread->error_code = 0;

    p_task->counter = 1;
    p_task->signal = 0;
    p_task->priority = 15;

    add_list_tail(ready_task_list, &p_task->task_node);
}