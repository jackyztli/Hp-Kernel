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

#endif