/*
 *  lib/string.h
 *
 *  (C) 2021  Jacky
 */
#ifndef STRING_H
#define STRING_H

#include "stdint.h"

/* 字符串按字节赋值 */
void memset(void *dst, uint8_t value, uint32_t size);
/* 内存拷贝 */
void memcpy(void *dst, const void *src, uint32_t size);
/* 源字符串拷贝到目的字符串 */
void strcpy(char *dst, const char *src);
/* 返回字符串长度 */
uint32_t strlen(const char *str);
/* 比较字符串 */
int8_t strcmp(const char *a, const char *b);
/* 拼接2个字符串 */
void strcat(char* a, const char* b);
/* 从后往前查找字符串str中首次出现字符ch的地址(不是下标,是地址) */
char* strrchr(const char *str, const uint8_t ch);

#endif