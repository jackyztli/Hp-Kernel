#include <task.h>
#include <stdint.h>

/* init任务，当前只支持单CPU，后续适配多CPU */
struct task_union init_task;

static inline uint64_t alloc_task_pid(void)
{
    static uint64_t pid = 0;
    return pid++;
}

/* 任务切换函数 */
static void switch_to(struct task_struct *cur, struct task_struct *next)
{
    __asm__ __volatile__(
        "movq  %%rsp, %0        \
         movq  %1,    %%rsp"
        :"=m"(cur->thread->rsp) :"m"(next->thread->rsp)
    );
}

/* 任务初始化 */
void init_task(void)
{
    /* 说明：
     * 每一个任务栈占32K的空间，低地址存放PCB(task_struct)信息，紧接着是内存管理(mm_struct)信息，再接着是线程相关(thread_struct)信息
     * 任务栈从高到低存放
     */
    struct task_struct *p_task = &init_task;
    p_task->pid = alloc_task_pid();
    p_task->state = TASK_RUNNING;
    p_task->flag = TASK_KERNEL;

    /* 初始化内存管理相关信息 */
    p_task->mm = (struct mm_struct *)((uintptr_t)p_task + sizeof(struct task_struct));
    p_task->mm->pgd = (uintptr_t)INIT_TASK_CR3;
    p_task->mm->start_code = (uintptr_t)&_text;
    p_task->mm->end_code = (uintptr_t)&_etext;
    p_task->mm->start_data = (uintptr_t)&_data;
    p_task->mm->end_code = (uintptr_t)&_edata;
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
}