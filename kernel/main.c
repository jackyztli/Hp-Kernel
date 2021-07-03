/*
 *  kernel/main.c
 *
 *  (C) 2021  Jacky
 */

#include "lib/print.h"
#include "kernel/panic.h"
#include "kernel/interrupt.h"
#include "kernel/device/timer.h"
#include "kernel/device/ide.h"
#include "kernel/memory.h"
#include "kernel/thread.h"
#include "kernel/console.h"
#include "kernel/process.h"
#include "kernel/tss.h"
#include "kernel/syscall.h"
#include "fs/fs.h"
#include "fs/file.h"

uint32_t g_procA = 0;
uint32_t g_procB = 0;

void ThreadA_Test(void *args);
void ThreadB_Test(void *args);
void ProcessA_Test(void);
void ProcessB_Test(void);

int main()
{
    put_str("I'm a Kernel\n");
	
	/* 初始化中断 */
	Idt_Init();

	/* 调整时钟中断周期 */
	Timer_Init();

	/* 初始化内存管理模块 */
	Mem_Init();
	
	/* 初始化打印控制台 */
	Console_Init();

	/* 任务初始化 */
    Thread_Init();

	TSS_Init();
	Syscall_Init();
		
	Process_Create(ProcessA_Test, "Process_1");
	//Process_Create(ProcessB_Test, "Process_2");

	Task *task1 = Thread_Create("test_1", 8,  ThreadA_Test, "Test_1 ");
	//Task *task2 = Thread_Create("test_2", 32, ThreadB_Test, "Test_2 ");

	/* 打开中断 */
	Idt_IntrEnable();

	Ide_Init();
	
	FS_Init();

	int32_t fd = sys_open("/file1", O_RDWR);
	Console_PutStr("fd: ");
	Console_PutInt(fd);
	Console_PutStr("\n");

	sys_write(fd, "hello world\n", 12);

	sys_close(fd);
	Console_PutStr("fd: ");
	Console_PutInt(fd);
	Console_PutStr(" closed now\n");

	init();

    return 0;
}

void init(void)
{
	pid_t pid = fork();
	while (1) {
		// Console_PutStr("Main ");
	}
}

void ThreadA_Test(void *args)
{
	Console_PutStr("ThreadA:0x");
	Console_PutInt(sys_getpid());
	Console_PutStr("\n");
	Console_PutStr("ProcessA:0x");
	Console_PutInt(g_procA);
	Console_PutStr("\n");

	while (1) {

	}
}

void ThreadB_Test(void *args)
{
	Console_PutStr("ThreadB:0x");
	Console_PutInt(sys_getpid());
	Console_PutStr("\n");
	Console_PutStr("ProcessB:0x");
	Console_PutInt(g_procB);
	Console_PutStr("\n");

	void *addr = malloc(1025);
	if (addr != NULL) {
		Console_PutStr("malloc 1 success, addr is 0x");
		Console_PutInt((uintptr_t)addr);
		Console_PutStr("\n");
	}

	free(addr);
	
	addr = malloc(2048);
	if (addr != NULL) {
		Console_PutStr("malloc 2 success, addr is 0x");
		Console_PutInt((uintptr_t)addr);
		Console_PutStr("\n");
	}
	
	while (1) {

	}
}

void ProcessA_Test(void)
{
	g_procA = getpid();
	write(STDOUT_NO, "In ProcessA_Test\n", 1024);
	while (1) {

	}
}

void ProcessB_Test(void)
{
	g_procB = getpid();
	write(STDOUT_NO, "In ProcessB_Test\n", 1024);
	while (1) {
		
	}
}
