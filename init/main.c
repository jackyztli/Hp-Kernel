#include <stdint.h>
#include <printk.h>
#include <trap.h>
#include <memory.h>
#include <interrupt.h>
#include <task.h>

uint64_t test(void *args);

void kernel_init(void)
{
    printk("kernel init...\n");

    /* 初始化系统异常处理 */
    init_trap();
    
    /* 内存初始化 */
    init_mem();

    /* 中断处理相关初始化 */
    init_interrupt();

    /* 开中断 */
    sti();

    /* 初始化tss */
    init_tss();

    /* 初始化init任务 */
    setup_task();

    /* 创建测试任务 */
    create_task(test, "test init...");

    while (1) {

    }
}

uint64_t test(void *args)
{
    printk("%s\n", (char *)args);
    return 0;
}