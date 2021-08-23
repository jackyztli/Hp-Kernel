#include <stdint.h>
#include <printk.h>

void kernel_init(void)
{
    printk("start kernel... %s\n", "ABCDEFGHIJKLMNOPQ,RSTUVWSYZ:\nzyswvutsr,qponmlkjihgfedcba");
    while (1) {

    }
}
