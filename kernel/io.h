/*
 *  kernel/io.h
 *
 *  (C) 2021  Jacky
 */
#ifndef IO_H
#define IO_H

#include "stdint.h"

/* 往指定端口写入一个值 */
static inline void outb(uint8_t port, uint8_t value)
{
    __asm__ volatile("outb %b0, %w1" :: "a"(value), "Nd"(port));
}

#endif