/*
 *  kernel/bitmap.c
 *
 *  (C) 2021  Jacky
 */

#include "bitmap.h"
#include "kernel/panic.h"
#include "lib/string.h"

/* 位图初始化函数 */
void BitmapInit(Bitmap *bitmap)
{
    /* 合法性校验 */
    ASSERT(bitmap != NULL);

    memset(bitmap->bitmap, 0, bitmap->bitmapLen);

    return;
}

/* 获取位图bitIndex位函数 */
uint32_t BitmapGet(Bitmap *bitmap, uint32_t bitIndex)
{
    /* 合法性校验 */
    ASSERT(bitmap != NULL);
    /* bitIndex不能超过总长度 */
    ASSERT(bitmap->bitmapLen > (bitIndex / 8));
    
    return (bitmap->bitmap[bitIndex / 8] & (1 << (bitIndex % 8))) >> (bitIndex % 8);
}

/* 设置位图bitIndex位函数 */
void BitmapSet(Bitmap *bitmap, uint32_t bitIndex, uint8_t value)
{
    /* 合法性校验 */
    ASSERT(bitmap != NULL);
    /* bitIndex不能超过总长度 */
    ASSERT(bitmap->bitmapLen > (bitIndex / 8));
    /* value非0即1 */
    ASSERT((value == 0) || (value == 1));

    if (value == 1) {
        bitmap->bitmap[bitIndex / 8] |= (1 << (bitIndex % 8)); 
    } else {
        bitmap->bitmap[bitIndex / 8] &= ~(1 << (bitIndex % 8)); 
    }

    return;
}

/* 第k位是否已经被使用 */
static bool BitmapBitIndexUsed(Bitmap *bitmap, uint32_t k)
{
    uint32_t index = k / 8;
    uint8_t bitMask = (1 << (k % 8));

    /* 第k位未被使用 */
    if (((bitmap->bitmap[index]) & bitMask) == 0) {
        return false;
    }

    return true;
}

/* 查找n个大小合适的空间，如果找到则返回起始index，否则返回-1 */
int32_t BitmapScan(Bitmap *bitmap, uint32_t n)
{
    /* bitmap位图中最多有(bitmap->bitmapLen + 1) * 8位 */
    for (uint32_t i = 0; i + n < ((bitmap->bitmapLen + 1) * 8); i++) {
        uint32_t cnt = 0;
        while (BitmapBitIndexUsed(bitmap, i + cnt) == false) {
            cnt++;
            /* 找到n个合适空间，返回起始index，即i */
            if (cnt >= n) {
                return i;
            }
        }

        /* 连续第i + cnt位已经被使用，下次从i + cnt下一位开始遍历 */
        i += cnt;
    }

    return -1;
}