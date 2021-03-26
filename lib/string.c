/*
 *  lib/string.c
 *
 *  (C) 2021  Jacky
 */

#include "string.h"
#include "kernel/panic.h"

/* 字符串按字节赋值 */
void memset(void *dst, uint8_t value, uint32_t size)
{
    ASSERT(dst != NULL);

    uint8_t *dstTmp = (uint8_t *)dst;
    while(size--) {
        *dstTmp = value;
        dstTmp++;
    }

    return; 
}