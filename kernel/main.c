/*
 *  kernel/main.c
 *
 *  (C) 2021  Jacky
 */

#include "lib/print.h"
#include "kernel/panic.h"
#include "kernel/interrupt.h"
#include "kernel/device/timer.h"
#include "kernel/memory.h"

int main()
{
    put_str("I'm a Kernel\n");
	
	/* 初始化中断 */
	Idt_Init();
	/* 打开中断 */
	__asm__ volatile("sti");

	/* 调整时钟中断周期 */
	Timer_Init();

	/* 初始化内存管理模块 */
	Mem_Init();

	/* 申请3内核页测试 */
	put_str("Mem_GetKernelPages1 start addr is ");
	put_int((uintptr_t)Mem_GetKernelPages(3));
	put_str("\n");

	put_str("Mem_GetKernelPages2 start addr is ");
	put_int((uintptr_t)Mem_GetKernelPages(5));
	put_str("\n");

	while (1) {
	
	}

    return 0;
}