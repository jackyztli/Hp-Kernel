/*
 *  kernel/bitmap.h
 *
 *  (C) 2021  Jacky
 */
#ifndef BITMAP_H
#define BITMAP_H

#include "stdint.h"

/* 位图结构体定义 */
typedef struct {
    uint32_t bitmapLen;
    uint8_t  *bitmap;
} Bitmap;

/* 位图初始化函数 */
void BitmapInit(Bitmap *bitmap);

/* 在位图中查找n个连续可用的bit */
int32_t BitmapScan(Bitmap *bitmap, uint32_t n);

/* 获取位图bitIndex位函数 */
uint32_t BitmapGet(Bitmap *bitmap, uint32_t bitIndex);

/* 设置位图bitIndex的值 */
void BitmapSet(Bitmap *bitmap, uint32_t bitIndex, uint8_t value);

#endif