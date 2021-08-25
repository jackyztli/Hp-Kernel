#include <stdint.h>
#include <printk.h>
#include <trap.h>

void kernel_init(void)
{
    printk("kernel init...\n");

    /* 初始化系统异常处理 */
    trap_init();

    /* 开中断 */
    sti();

    /* 测试中断 */
    uint32_t i = 10 / 0;
    
    while (1) {

    }
}
