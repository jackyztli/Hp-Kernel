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
#include "kernel/thread.h"
#include "kernel/console.h"
#include "kernel/tss.h"

void Thread_Test(void *args)
{
	while (1) {
		Console_PutStr((const char *)args);
	}
}

int main()
{
    put_str("I'm a Kernel\n");
	
	/* 初始化中断 */
	Idt_Init();

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
	
	/* 初始化打印控制台 */
	Console_Init();

	/* 任务初始化 */
    Thread_Init();

	Task *task1 = Thread_Create("test_1", 8,  Thread_Test, "Test_1 ");
	Task *task2 = Thread_Create("test_2", 32, Thread_Test, "Test_2 ");

	TSS_Init();
	
	/* 打开中断 */
	Idt_IntrEnable();

	while (1) {
		Console_PutStr("Main ");
	}

    return 0;
}