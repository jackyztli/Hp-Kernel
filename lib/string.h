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
/* 源字符串拷贝到目的字符串 */
void strcpy(char *dst, const char *src);

#endif