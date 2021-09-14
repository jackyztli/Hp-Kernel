#include <task.h>
#include <stdint.h>
#include <memory.h>
#include <printk.h>

#include <list.h>

/* init任务，当前只支持单CPU，后续适配多CPU */
union task_union init_task;

struct list *ready_task_list;

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
        "popq %rax;"                \
        "movq %rax, %ds;"           \
        "popq %rax;"                \
        "movq %rax, %es;"           \
        "popq %rbx;"                \
        "popq %rdi;"                \
        "movq %rsp, %rbp;"          \
        "callq *%rbx;"               \
        "movq %rax, %rdi;"          \
        "callq do_exit;"
    );
}

/* 创建任务入口 */
pid_t create_task(uint64_t (* func)(void *), void *args)
{
    uintptr_t page = alloc_page();
    if (page == NULL) {
        printk("no enough memory!!!");
        return -1;
    } 

    union task_union *new_union = (union task_union *)page;
    struct task_struct *new_task = &new_union->task;
    new_task->pid = alloc_task_pid();
    new_task->task_node.next = NULL;
    new_task->state = TASK_RUNNING;
    new_task->flag = TASK_KERNEL;

    new_task->mm = (struct mm_struct *)((uintptr_t)new_task + sizeof(struct task_struct));
    new_task->thread = (struct thread_struct *)((uintptr_t)new_task->mm + sizeof(struct mm_struct));
    new_task->thread->rsp0 = (uintptr_t)new_task + STACK_BYTES;
    new_task->thread->rip = (uintptr_t)kernel_thread_start;
    new_task->thread->rsp = (uintptr_t)new_task + STACK_BYTES - sizeof(struct pt_regs);
    new_task->thread->fs = KERNEL_DS;
    new_task->thread->gs = KERNEL_DS;
    new_task->thread->cr2 = 0;
    new_task->thread->trap_nr = 0;
    new_task->thread->error_code = 0;

    new_task->counter = 1;
    new_task->signal = 0;
    new_task->priority = 15;

    struct pt_regs *new_pt_regs = (struct pt_regs *)new_task->thread->rsp;
    new_pt_regs->ds = KERNEL_DS;
    new_pt_regs->es = KERNEL_DS;
    new_pt_regs->rbx = (uintptr_t)func;
    new_pt_regs->rdi = (uintptr_t)args;

    add_list_tail(ready_task_list, &new_task->task_node);

    return 0;
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