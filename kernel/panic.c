/*
 *  kernel/panic.c
 *
 *  (C) 2021  Jacky
 */

#include "panic.h"
#include "kernel/interrupt.h"
#include "lib/print.h"

void panic(const char *fileName, uint32_t fileLine, const char *funcName, const char *condition)
{
    /* 关中断 */
    Idt_IntrDisable();

    put_str("\n\n error!!! \n\n");
    put_str("filename: ");
    put_str(fileName);
    put_str("\n");

    put_str("line: ");
    put_int(fileLine);
    put_str("\n");

    put_str("func: ");
    put_str(funcName);
    put_str("\n");

    put_str("condition: ");
    put_str(condition);
    put_str("\n");

    /* 发生panic后，系统挂起 */
    while (1) {}
}