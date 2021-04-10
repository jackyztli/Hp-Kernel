/*
 *  kernel/console.h
 *
 *  (C) 2021  Jacky
 */
#ifndef CONSOLE_H
#define CONSOLE_H

#include "stdint.h"

/* 控制台打印字符串 */
void Console_PutStr(const char *str);
/* 控制台打印字符 */
void Console_PutChar(char c);
/* 控制台打印数字 */
void Console_PutInt(int32_t num);
/* 控制台初始化 */
void Console_Init(void);

#endif