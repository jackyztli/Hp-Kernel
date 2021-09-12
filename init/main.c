#include <stdint.h>
#include <printk.h>
#include <trap.h>
#include <memory.h>
#include <interrupt.h>

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

    while (1) {

    }
}
