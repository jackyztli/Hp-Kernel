/*
 *  kernel/panic.h
 *
 *  (C) 2021  Jacky
 */

#ifndef PANIC_H
#define PANIC_H

#include "stdint.h"

void panic(const char *fileName, uint32_t fileLine, const char *funcName, const char *condition);

#ifdef DEBUG_KERNEL
    #define ASSERT(CONDITION) ((void)0)
#else
#define ASSERT(CONDITION) \
    do {                                                           \
        if (CONDITION) {                                           \
        } else {                                                   \
            panic(__FILE__, __LINE__, __func__, #CONDITION);   \
        }                                                          \
    } while (0)
#endif

#endif