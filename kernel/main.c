/*
 *  kernel/main.c
 *
 *  (C) 2021  Jacky
 */

#include "lib/print.h"
#include "interrupt.h"

int main()
{
    put_str("I'm a Kernel\n");
	
	Idt_Init();
	/* 打开中断 */
	__asm__ volatile("sti");

	while (1) {
	
	}

    return 0;
}