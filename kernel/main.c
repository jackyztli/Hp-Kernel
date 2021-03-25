/*
 *  kernel/main.c
 *
 *  (C) 2021  Jacky
 */

#include "lib/print.h"
#include "kernel/interrupt.h"
#include "kernel/device/timer.h"

int main()
{
    put_str("I'm a Kernel\n");
	
	/* 初始化中断 */
	Idt_Init();
	/* 打开中断 */
	__asm__ volatile("sti");

	/* 调整时钟中断周期 */
	Timer_Init();

	while (1) {
	
	}

    return 0;
}