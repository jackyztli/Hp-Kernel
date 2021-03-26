/*
 *  include/stdint.h
 *
 *  (C) 2021  Jacky
 */
#ifndef STDINT_H
#define STDINT_H

/* 默认运行环境是32位， 后续支持64位环境需要扩展 */
typedef unsigned char             uint8_t;
typedef unsigned short            uint16_t;
typedef unsigned int              uint32_t;
typedef unsigned long long int    uint64_t;

typedef unsigned int              uintptr_t;

typedef signed char               int8_t;
typedef signed short              int16_t;
typedef signed int                int32_t;
typedef signed long long int      int64_t;

typedef enum {
    false = 0,
    true = 1
} bool;

#define NULL 0

#endif