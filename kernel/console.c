/*
 *  kernel/console.c
 *
 *  (C) 2021  Jacky
 */

#include "console.h"
#include "kernel/sync.h"
#include "lib/print.h"

static Lock consoleLock;

/* 控制台打印字符串 */
void Console_PutStr(const char *str)
{
    Lock_Lock(&consoleLock);
    put_str(str);
    Lock_UnLock(&consoleLock);

    return;
}

/* 控制台打印字符 */
void Console_PutChar(char c)
{
    Lock_Lock(&consoleLock);
    put_char(c);
    Lock_UnLock(&consoleLock);

    return;
}

/* 控制台打印数字 */
void Console_PutInt(int32_t num)
{
    Lock_Lock(&consoleLock);
    put_int(num);
    Lock_UnLock(&consoleLock);

    return;
}

/* 控制台初始化 */
void Console_Init(void)
{
    Lock_Init(&consoleLock);

    return;
}